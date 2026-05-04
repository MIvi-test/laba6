#define _POSIX_C_SOURCE 200809L

#include "hash_table.h"

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double elapsed_ms(struct timespec start, struct timespec end)
{
    long seconds = end.tv_sec - start.tv_sec;
    long nsec = end.tv_nsec - start.tv_nsec;
    return (double)seconds * 1000.0 + (double)nsec / 1000000.0;
}

static uint64_t xorshift64star(uint64_t *state)
{
    uint64_t x = *state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return x * 2685821657736338717ULL;
}

static char *make_key(size_t len, uint64_t *rng)
{
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    char *s = malloc(len + 1);
    if (s == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < len; i++)
    {
        uint64_t r = xorshift64star(rng);
        s[i] = alphabet[r % (sizeof(alphabet) - 1)];
    }
    s[len] = '\0';
    return s;
}

static bool generate_keys(char ***out_keys, size_t count, size_t len, uint64_t seed)
{
    char **keys = calloc(count, sizeof(char *));
    if (keys == NULL)
    {
        return false;
    }

    uint64_t rng = seed ? seed : 0x123456789abcdef0ULL;

    for (size_t i = 0; i < count; i++)
    {
        keys[i] = make_key(len, &rng);
        if (keys[i] == NULL)
        {
            for (size_t j = 0; j < i; j++)
            {
                free(keys[j]);
            }
            free(keys);
            return false;
        }
    }

    *out_keys = keys;
    return true;
}

static void free_keys(char **keys, size_t count)
{
    if (keys == NULL)
    {
        return;
    }
    for (size_t i = 0; i < count; i++)
    {
        free(keys[i]);
    }
    free(keys);
}

static bool parse_size_t(const char *s, size_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long v = strtoull(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0')
    {
        return false;
    }
    if (v > (unsigned long long)SIZE_MAX)
    {
        return false;
    }
    *out = (size_t)v;
    return true;
}

static bool parse_double(const char *s, double *out)
{
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (errno != 0 || end == s || *end != '\0')
    {
        return false;
    }
    *out = v;
    return true;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s [N] [key_len] [min_thr] [max_thr] [step] [repeats]\n"
            "\n"
            "Defaults:\n"
            "  N        = 200000\n"
            "  key_len  = 15\n"
            "  min_thr  = 0.50\n"
            "  max_thr  = 0.95\n"
            "  step     = 0.05\n"
            "  repeats  = 3\n"
            "\n"
            "Output: CSV to stdout (threshold,insert_ms,lookup_ms,total_ms,collisions,resizes)\n",
            argv0);
}

int main(int argc, char **argv)
{
    size_t n = 200000;
    size_t key_len = 15;
    double min_thr = 0.50;
    double max_thr = 0.95;
    double step = 0.05;
    size_t repeats = 3;

    if (argc > 1 && !parse_size_t(argv[1], &n))
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 2 && !parse_size_t(argv[2], &key_len))
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 3 && !parse_double(argv[3], &min_thr))
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 4 && !parse_double(argv[4], &max_thr))
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 5 && !parse_double(argv[5], &step))
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 6 && !parse_size_t(argv[6], &repeats))
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (argc > 7 || n == 0 || key_len == 0 || repeats == 0 || step <= 0.0 || min_thr <= 0.0 || max_thr > 1.0 ||
        min_thr > max_thr)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    char **keys = NULL;
    if (!generate_keys(&keys, n, key_len, 0xfeedbeefULL))
    {
        fprintf(stderr, "Failed to generate keys.\n");
        return EXIT_FAILURE;
    }

    fprintf(stdout, "threshold,insert_ms,lookup_ms,total_ms,collisions,resizes\n");

    double best_thr = min_thr;
    double best_total = 1e300;

    for (double thr = min_thr; thr <= max_thr + 1e-12; thr += step)
    {
        double sum_insert = 0.0;
        double sum_lookup = 0.0;
        uint64_t sum_collisions = 0;
        uint64_t sum_resizes = 0;

        for (size_t r = 0; r < repeats; r++)
        {
            HashTable table;
            if (!ht_init(&table, 8, thr))
            {
                fprintf(stderr, "ht_init failed for threshold %.4f\n", thr);
                free_keys(keys, n);
                return EXIT_FAILURE;
            }

            struct timespec t0, t1, t2;
            (void)clock_gettime(CLOCK_MONOTONIC, &t0);

            for (size_t i = 0; i < n; i++)
            {
                if (!ht_put(&table, keys[i], (int)i))
                {
                    fprintf(stderr, "ht_put failed (threshold=%.4f, i=%zu)\n", thr, i);
                    ht_destroy(&table);
                    free_keys(keys, n);
                    return EXIT_FAILURE;
                }
            }

            (void)clock_gettime(CLOCK_MONOTONIC, &t1);

            int value = 0;
            for (size_t i = 0; i < n; i++)
            {
                if (!ht_get(&table, keys[i], &value))
                {
                    fprintf(stderr, "ht_get failed (threshold=%.4f, i=%zu)\n", thr, i);
                    ht_destroy(&table);
                    free_keys(keys, n);
                    return EXIT_FAILURE;
                }
            }

            (void)clock_gettime(CLOCK_MONOTONIC, &t2);

            sum_insert += elapsed_ms(t0, t1);
            sum_lookup += elapsed_ms(t1, t2);
            sum_collisions += table.collision_count;
            sum_resizes += table.resize_count;
            ht_destroy(&table);
        }

        double avg_insert = sum_insert / (double)repeats;
        double avg_lookup = sum_lookup / (double)repeats;
        double avg_total = avg_insert + avg_lookup;
        uint64_t avg_collisions = sum_collisions / repeats;
        uint64_t avg_resizes = sum_resizes / repeats;

        fprintf(stdout, "%.4f,%.3f,%.3f,%.3f,%" PRIu64 ",%" PRIu64 "\n", thr, avg_insert, avg_lookup, avg_total,
                avg_collisions, avg_resizes);

        if (avg_total < best_total)
        {
            best_total = avg_total;
            best_thr = thr;
        }
    }

    fprintf(stderr, "BEST threshold=%.4f total_ms=%.3f (N=%zu key_len=%zu repeats=%zu)\n", best_thr, best_total, n,
            key_len, repeats);

    free_keys(keys, n);
    return EXIT_SUCCESS;
}

