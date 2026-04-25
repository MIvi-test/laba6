#include "hash_table.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 8U
#define LOAD_FACTOR_THRESHOLD 0.7
#define MAX_LINE_LENGTH 4096
#define MAX_KEY_LENGTH 1024

static char *trim(char *string) {
    char *end = NULL;

    while (*string != '\0' && isspace((unsigned char)*string)) {
        string++;
    }

    if (*string == '\0') {
        return string;
    }

    end = string + strlen(string) - 1;
    while (end >= string && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }

    return string;
}

static bool parse_key_command(char *arguments, char *key, size_t key_size) {
    char *normalized = trim(arguments);
    size_t length = strlen(normalized);

    if (length == 0 || length >= key_size) {
        return false;
    }

    memcpy(key, normalized, length + 1);
    return true;
}

static bool parse_put_command(char *arguments, char *key, size_t key_size, int *value) {
    char *comma = strchr(arguments, ',');
    char *key_part = NULL;
    char *value_part = NULL;
    char *end_ptr = NULL;
    long parsed_value = 0;

    if (comma == NULL) {
        return false;
    }

    *comma = '\0';
    key_part = trim(arguments);
    value_part = trim(comma + 1);

    if (*key_part == '\0' || *value_part == '\0') {
        return false;
    }

    if (strlen(key_part) >= key_size) {
        return false;
    }

    parsed_value = strtol(value_part, &end_ptr, 10);
    if (*end_ptr != '\0' || parsed_value < INT_MIN || parsed_value > INT_MAX) {
        return false;
    }

    strcpy(key, key_part);
    *value = (int)parsed_value;
    return true;
}

int main(void) {
    HashTable table;
    char line[MAX_LINE_LENGTH];
    char key[MAX_KEY_LENGTH];
    int value = 0;

    if (!ht_init(&table, INITIAL_CAPACITY, LOAD_FACTOR_THRESHOLD)) {
        fprintf(stderr, "Failed to initialize hash table.\n");
        return EXIT_FAILURE;
    }

    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *command = trim(line);

        if (*command == '\0') {
            continue;
        }

        if (strncmp(command, "put", 3) == 0 && isspace((unsigned char)command[3])) {
            if (!parse_put_command(command + 4, key, sizeof(key), &value)) {
                fprintf(stderr, "Invalid command.\n");
                continue;
            }

            if (!ht_put(&table, key, value)) {
                fprintf(stderr, "Failed to insert value.\n");
                ht_destroy(&table);
                return EXIT_FAILURE;
            }

            continue;
        }

        if (strncmp(command, "get", 3) == 0 && isspace((unsigned char)command[3])) {
            if (!parse_key_command(command + 4, key, sizeof(key))) {
                fprintf(stderr, "Invalid command.\n");
                continue;
            }

            if (ht_get(&table, key, &value)) {
                printf("%d\n", value);
            } else {
                printf("Key not found\n");
            }

            continue;
        }

        if (strncmp(command, "del", 3) == 0 && isspace((unsigned char)command[3])) {
            if (!parse_key_command(command + 4, key, sizeof(key))) {
                fprintf(stderr, "Invalid command.\n");
                continue;
            }

            if (ht_delete(&table, key, &value)) {
                printf("%d\n", value);
            } else {
                printf("Key not found\n");
            }

            continue;
        }

        fprintf(stderr, "Invalid command.\n");
    }

    ht_destroy(&table);
    return EXIT_SUCCESS;
}
