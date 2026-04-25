#include "hash_table.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(condition)                                                      \
    do {                                                                            \
        if (!(condition)) {                                                         \
            fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #condition, __FILE__, \
                    __LINE__);                                                      \
            return false;                                                           \
        }                                                                           \
    } while (0)

static bool test_put_get_and_replace(void) {
    HashTable table;
    int value = 0;

    ASSERT_TRUE(ht_init(&table, 8, 0.7));
    ASSERT_TRUE(ht_put(&table, "alpha", 10));
    ASSERT_TRUE(ht_get(&table, "alpha", &value));
    ASSERT_TRUE(value == 10);

    ASSERT_TRUE(ht_put(&table, "alpha", 25));
    ASSERT_TRUE(ht_get(&table, "alpha", &value));
    ASSERT_TRUE(value == 25);
    ASSERT_TRUE(table.size == 1);

    ht_destroy(&table);
    return true;
}

static bool test_delete_and_missing_key(void) {
    HashTable table;
    int value = 0;

    ASSERT_TRUE(ht_init(&table, 8, 0.7));
    ASSERT_TRUE(ht_put(&table, "beta", 7));
    ASSERT_TRUE(ht_delete(&table, "beta", &value));
    ASSERT_TRUE(value == 7);
    ASSERT_TRUE(!ht_get(&table, "beta", &value));
    ASSERT_TRUE(!ht_delete(&table, "beta", &value));

    ht_destroy(&table);
    return true;
}

static bool test_resize_preserves_values(void) {
    HashTable table;
    int value = 0;

    ASSERT_TRUE(ht_init(&table, 8, 0.7));
    ASSERT_TRUE(ht_put(&table, "one", 1));
    ASSERT_TRUE(ht_put(&table, "two", 2));
    ASSERT_TRUE(ht_put(&table, "three", 3));
    ASSERT_TRUE(ht_put(&table, "four", 4));
    ASSERT_TRUE(ht_put(&table, "five", 5));
    ASSERT_TRUE(ht_put(&table, "six", 6));
    ASSERT_TRUE(table.capacity == 16);
    ASSERT_TRUE(table.resize_count == 1);

    ASSERT_TRUE(ht_get(&table, "one", &value));
    ASSERT_TRUE(value == 1);
    ASSERT_TRUE(ht_get(&table, "six", &value));
    ASSERT_TRUE(value == 6);

    ht_destroy(&table);
    return true;
}

static bool test_linear_probing_collision_handling(void) {
    HashTable table;
    int value = 0;

    ASSERT_TRUE(ht_init(&table, 2, 1.0));
    ASSERT_TRUE(ht_put(&table, "first", 11));
    ASSERT_TRUE(ht_put(&table, "second", 22));
    ASSERT_TRUE(table.collision_count > 0);
    ASSERT_TRUE(ht_get(&table, "first", &value));
    ASSERT_TRUE(value == 11);
    ASSERT_TRUE(ht_get(&table, "second", &value));
    ASSERT_TRUE(value == 22);

    ht_destroy(&table);
    return true;
}

static bool test_deleted_slot_can_be_reused(void) {
    HashTable table;
    int value = 0;

    ASSERT_TRUE(ht_init(&table, 4, 1.0));
    ASSERT_TRUE(ht_put(&table, "x", 1));
    ASSERT_TRUE(ht_put(&table, "y", 2));
    ASSERT_TRUE(ht_delete(&table, "x", &value));
    ASSERT_TRUE(value == 1);
    ASSERT_TRUE(ht_put(&table, "z", 3));
    ASSERT_TRUE(ht_get(&table, "z", &value));
    ASSERT_TRUE(value == 3);
    ASSERT_TRUE(table.size == 2);

    ht_destroy(&table);
    return true;
}

int main(void) {
    ASSERT_TRUE(test_put_get_and_replace());
    ASSERT_TRUE(test_delete_and_missing_key());
    ASSERT_TRUE(test_resize_preserves_values());
    ASSERT_TRUE(test_linear_probing_collision_handling());
    ASSERT_TRUE(test_deleted_slot_can_be_reused());

    printf("All tests passed.\n");
    return EXIT_SUCCESS;
}
