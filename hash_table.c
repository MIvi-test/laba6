#include "hash_table.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *source)
{
    size_t length = strlen(source);
    char *copy = malloc(length + 1);

    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, source, length + 1);
    return copy;
}

static unsigned int sdbm_hash(const char *string)
{
    unsigned int hash = 0;

    while (*string != '\0')
    {
        hash = (unsigned char)(*string) + (hash << 6) + (hash << 16) - hash;
        string++;
    }

    return hash;
}

static bool should_resize(const HashTable *table, size_t new_size)
{
    return ((double)new_size / (double)table->capacity) > table->load_factor_threshold; // haha смотрите неточности
}

static size_t find_slot(const HashTable *table, const char *key, bool *found)
{
    size_t start_index = sdbm_hash(key) % table->capacity; // becket
    size_t first_deleted = table->capacity;

    *found = false;

    for (size_t step = 0; step < table->capacity; step++)
    {
        size_t index = (start_index + step) % table->capacity;
        const HashEntry *entry = &table->entries[index];

        if (entry->state == SLOT_EMPTY)
        {
            if (first_deleted != table->capacity)
            {
                return first_deleted;
            }

            return index;
        }

        if (entry->state == SLOT_DELETED)
        {
            if (first_deleted == table->capacity)
            {
                first_deleted = index;
            }

            continue;
        }

        if (strcmp(entry->key, key) == 0)
        {
            *found = true;
            return index;
        }
    }

    return first_deleted;
}

static bool insert_owned(HashTable *table, char *key, int value, bool count_collisions)
{
    size_t start_index = sdbm_hash(key) % table->capacity;
    size_t first_deleted = table->capacity;

    for (size_t step = 0; step < table->capacity; step++)
    {
        size_t index = (start_index + step) % table->capacity;
        HashEntry *entry = &table->entries[index];

        if (entry->state == SLOT_OCCUPIED)
        {
            if (count_collisions)
            {
                table->collision_count++;
            }

            continue;
        }

        if (entry->state == SLOT_DELETED)
        {
            if (first_deleted == table->capacity)
            {
                first_deleted = index;
            }

            continue;
        }

        if (first_deleted != table->capacity)
        {
            entry = &table->entries[first_deleted];
        }

        entry->key = key;
        entry->value = value;
        entry->state = SLOT_OCCUPIED;
        table->size++;
        return true;
    }

    if (first_deleted == table->capacity)
    {
        return false;
    }

    table->entries[first_deleted].key = key;
    table->entries[first_deleted].value = value;
    table->entries[first_deleted].state = SLOT_OCCUPIED;
    table->size++;
    return true;
}

static bool ht_resize(HashTable *table, size_t new_capacity)
{
    HashEntry *old_entries = table->entries;
    size_t old_capacity = table->capacity;
    HashEntry *new_entries = calloc(new_capacity, sizeof(HashEntry));

    if (new_entries == NULL)
    {
        return false;
    }

    table->entries = new_entries;
    table->capacity = new_capacity;
    table->size = 0;

    for (size_t i = 0; i < old_capacity; i++)
    {
        if (old_entries[i].state == SLOT_OCCUPIED)
        {
            if (!insert_owned(table, old_entries[i].key, old_entries[i].value, false))
            {
                for (size_t j = 0; j < table->capacity; j++)
                {
                    if (table->entries[j].state == SLOT_OCCUPIED)
                    {
                        free(table->entries[j].key);
                    }
                }

                free(table->entries);
                table->entries = old_entries;
                table->capacity = old_capacity;

                table->size = 0;
                for (size_t j = 0; j < old_capacity; j++)
                {
                    if (old_entries[j].state == SLOT_OCCUPIED)
                    {
                        table->size++;
                    }
                }

                return false;
            }
        }
    }

    free(old_entries);
    table->resize_count++;
    return true;
}

bool ht_init(HashTable *table, size_t initial_capacity, double load_factor_threshold)
{
    if (table == NULL || initial_capacity == 0)
    {
        return false;
    }

    if (load_factor_threshold <= 0.0 || load_factor_threshold > 1.0)
    {
        return false;
    }

    table->entries = calloc(initial_capacity, sizeof(HashEntry));
    if (table->entries == NULL)
    {
        return false;
    }

    table->size = 0;
    table->capacity = initial_capacity;
    table->load_factor_threshold = load_factor_threshold;
    table->resize_count = 0;
    table->collision_count = 0;
    return true;
}

void ht_destroy(HashTable *table)
{
    if (table == NULL || table->entries == NULL)
    {
        return;
    }

    for (size_t i = 0; i < table->capacity; i++)
    {
        if (table->entries[i].state == SLOT_OCCUPIED)
        {
            free(table->entries[i].key);
        }
    }

    free(table->entries);
    table->entries = NULL;
    table->size = 0;
    table->capacity = 0;
    table->resize_count = 0;
    table->collision_count = 0;
}

bool ht_put(HashTable *table, const char *key, int value)
{
    bool found = false;
    size_t index = 0;
    char *key_copy = NULL;

    if (table == NULL || table->entries == NULL || key == NULL)
    {
        return false;
    }

    index = find_slot(table, key, &found);
    if (found)
    {
        table->entries[index].value = value;
        return true;
    }

    if (should_resize(table, table->size + 1))
    {
        size_t new_capacity = table->capacity * 2; // haha UB
        if (new_capacity < table->capacity)
        {
            return false; // но я очень хотел написать кратко, так что впадлу 
        }

        if (!ht_resize(table, new_capacity))
        {
            return false;
        }
    }

    key_copy = duplicate_string(key);
    if (key_copy == NULL)
    {
        return false;
    }

    if (!insert_owned(table, key_copy, value, true))
    {
        free(key_copy);
        return false;
    }

    return true;
}

bool ht_get(const HashTable *table, const char *key, int *value)
{
    size_t start_index;

    if (table == NULL || table->entries == NULL || key == NULL || value == NULL)
    {
        return false;
    }

    start_index = sdbm_hash(key) % table->capacity;

    for (size_t step = 0; step < table->capacity; step++)
    {
        size_t index = (start_index + step) % table->capacity;
        HashEntry *entry = &table->entries[index];

        if (entry->state == SLOT_EMPTY)
        {
            return false;
        }

        if (entry->state == SLOT_OCCUPIED && strcmp(entry->key, key) == 0)
        {
            *value = entry->value;
            return true;
        }
    }

    return false;
}

bool ht_delete(HashTable *table, const char *key, int *value)
{
    size_t start_index;

    if (table == NULL || table->entries == NULL || key == NULL || value == NULL)
    {
        return false;
    }

    start_index = sdbm_hash(key) % table->capacity;

    for (size_t step = 0; step < table->capacity; step++)
    {
        size_t index = (start_index + step) % table->capacity;
        HashEntry *entry = &table->entries[index];

        if (entry->state == SLOT_EMPTY)
        {
            return false;
        }

        if (entry->state == SLOT_OCCUPIED && strcmp(entry->key, key) == 0)
        {
            *value = entry->value;
            free(entry->key);
            entry->key = NULL;
            entry->state = SLOT_DELETED;
            table->size--;
            return true;
        }
    }

    return false;
}

double ht_current_load_factor(const HashTable *table)
{
    if (table == NULL || table->capacity == 0)
    {
        return 0.0;
    }

    return (double)table->size / (double)table->capacity;
}
