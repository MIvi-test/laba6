#define _POSIX_C_SOURCE 200809L

#include "hash_table.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INITIAL_CAPACITY 8U
#define LOAD_FACTOR_THRESHOLD 0.7
#define MAX_LINE_LENGTH 4096
#define MAX_KEY_LENGTH 1024

static char *trim(char *string)
{
    char *end = NULL;

    while (*string != '\0' && isspace((unsigned char)*string))
    {
        string++;
    }

    if (*string == '\0')
    {
        return string;
    }

    end = string + strlen(string) - 1;
    while (end >= string && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }

    return string;
}

static bool parse_key_command(char *arguments, char *key, size_t key_size)
{
    char *normalized = trim(arguments);
    size_t length = strlen(normalized);

    if (length == 0 || length >= key_size)
    {
        return false;
    }

    memcpy(key, normalized, length + 1);
    return true;
}

static bool parse_put_command(char *arguments, char *key, size_t key_size, int *value)
{
    char *equals = strchr(arguments, '=');
    char *key_part = NULL;
    char *value_part = NULL;
    char *end_ptr = NULL;
    long long parsed_value = 0;

    if (equals == NULL)
    {
        return false;
    }

    *equals = '\0';
    key_part = trim(arguments);
    value_part = trim(equals + 1);

    if (*key_part == '\0' || *value_part == '\0')
    {
        return false;
    }

    if (strlen(key_part) >= key_size)
    {
        return false;
    }

    parsed_value = strtol(value_part, &end_ptr, 10);
    if (*end_ptr != '\0' || parsed_value < INT_MIN || parsed_value > INT_MAX)
    {
        return false;
    }

    strcpy(key, key_part);
    *value = (int)parsed_value;
    return true;
}

static void print_help()
{
    printf("Available commands:\n");
    printf("  put <key> = <val>  - Insert or update a key-value pair\n");
    printf("                        Examples: put a = 10   |   put a=10\n");
    printf("  get <key>          - Retrieve value by key\n");
    printf("                        Example: get name\n");
    printf("  del <key>          - Delete key-value pair and return value\n");
    printf("                        Example: del name\n");
    printf("  load <file>        - Execute commands from file\n");
    printf("                        Example: load file.txt\n");
    printf("  help               - Show this help message\n");
    printf("  exit or Ctrl+D     - Exit the program\n");
    printf("\n");
    printf("Notes:\n");
    printf("  - Keys are case-sensitive strings\n");
    printf("  - Values must be integers (range: %d to %d)\n", INT_MIN, INT_MAX);
    printf("  - Empty lines are ignored\n");
    printf("  - Whitespace around keys and values is trimmed\n");
}

typedef enum CommandResult
{
    COMMAND_OK = 0,
    COMMAND_EXIT = 1,
    COMMAND_FATAL = 2
} CommandResult;

static double elapsed_ms(struct timespec start, struct timespec end)
{
    long seconds = end.tv_sec - start.tv_sec;
    long nsec = end.tv_nsec - start.tv_nsec;
    return (double)seconds * 1000.0 + (double)nsec / 1000000.0;
}

static CommandResult process_command(HashTable *table, char *line, int load_depth);
static CommandResult process_stream(HashTable *table, FILE *input, int load_depth);

static CommandResult process_stream(HashTable *table, FILE *input, int load_depth)
{
    char line[MAX_LINE_LENGTH];

    while (fgets(line, sizeof(line), input) != NULL)
    {
        CommandResult result = process_command(table, line, load_depth);
        if (result != COMMAND_OK)
        {
            return result;
        }
    }

    return COMMAND_OK;
}

static CommandResult process_command(HashTable *table, char *line, int load_depth)
{
    char key[MAX_KEY_LENGTH];
    int value = 0;
    char path[MAX_LINE_LENGTH];
    char *command = trim(line);

    if (*command == '\0')
    {
        return COMMAND_OK;
    }

    if (strncmp(command, "put", 3) == 0 && isspace((unsigned char)command[3]))
    {
        if (!parse_put_command(command + 4, key, sizeof(key), &value))
        {
            fprintf(stderr, "Invalid command.\n");
            return COMMAND_OK;
        }

        if (!ht_put(table, key, value))
        {
            fprintf(stderr, "Failed to insert value.\n");
            return COMMAND_FATAL;
        }

        return COMMAND_OK;
    }

    if (strncmp(command, "get", 3) == 0 && isspace((unsigned char)command[3]))
    {
        if (!parse_key_command(command + 4, key, sizeof(key)))
        {
            fprintf(stderr, "Invalid command.\n");
            return COMMAND_OK;
        }

        if (ht_get(table, key, &value))
        {
            printf("%d\n", value);
        }
        else
        {
            printf("Key not found\n");
        }

        return COMMAND_OK;
    }

    if (strncmp(command, "del", 3) == 0 && isspace((unsigned char)command[3]))
    {
        if (!parse_key_command(command + 4, key, sizeof(key)))
        {
            fprintf(stderr, "Invalid command.\n");
            return COMMAND_OK;
        }

        if (ht_delete(table, key, &value))
        {
            printf("%d\n", value);
        }
        else
        {
            printf("Key not found\n");
        }

        return COMMAND_OK;
    }

    if (strncmp(command, "load", 4) == 0 && isspace((unsigned char)command[4]))
    {
        if (load_depth >= 16)
        {
            fprintf(stderr, "Load nesting too deep.\n");
            return COMMAND_OK;
        }

        if (!parse_key_command(command + 5, path, sizeof(path)))
        {
            fprintf(stderr, "Invalid command.\n");
            return COMMAND_OK;
        }

        struct timespec start_time;
        struct timespec end_time;
        bool have_start_time = (clock_gettime(CLOCK_MONOTONIC, &start_time) == 0);

        FILE *input = fopen(path, "r");
        if (input == NULL)
        {
            perror("Failed to open file");
            return COMMAND_OK;
        }

        CommandResult result = process_stream(table, input, load_depth + 1);
        fclose(input);

        if (have_start_time && clock_gettime(CLOCK_MONOTONIC, &end_time) == 0)
        {
            fprintf(stderr, "Loaded '%s' in %.3f ms\n", path, elapsed_ms(start_time, end_time));
        }

        return result;
    }

    if (strcmp(command, "help") == 0)
    {
        print_help();
        return COMMAND_OK;
    }

    if (strcmp(command, "exit") == 0)
    {
        return COMMAND_EXIT;
    }

    fprintf(stderr, "Invalid command.\n");
    return COMMAND_OK;
}

int main(void)
{
    HashTable table;

    if (!ht_init(&table, INITIAL_CAPACITY, LOAD_FACTOR_THRESHOLD))
    {
        fprintf(stderr, "Failed to initialize hash table.\n");
        return EXIT_FAILURE;
    }

    print_help();
    fflush(stdout);
    CommandResult result = process_stream(&table, stdin, 0);
    ht_destroy(&table);

    return (result == COMMAND_FATAL) ? EXIT_FAILURE : EXIT_SUCCESS;
}
