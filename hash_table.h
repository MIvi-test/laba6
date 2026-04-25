#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum SlotState {
    SLOT_EMPTY = 0,
    SLOT_OCCUPIED = 1,
    SLOT_DELETED = 2
} SlotState;

typedef struct HashEntry {
    char *key;
    int value;
    SlotState state;
} HashEntry;

typedef struct HashTable {
    HashEntry *entries;
    size_t size;
    size_t capacity;
    double load_factor_threshold;
    size_t resize_count;
    size_t collision_count;
} HashTable;

bool ht_init(HashTable *table, size_t initial_capacity, double load_factor_threshold);
void ht_destroy(HashTable *table);

bool ht_put(HashTable *table, const char *key, int value);
bool ht_get(const HashTable *table, const char *key, int *value);
bool ht_delete(HashTable *table, const char *key, int *value);

double ht_current_load_factor(const HashTable *table);

#endif
