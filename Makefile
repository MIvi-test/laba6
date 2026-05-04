CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
APP = hash_cli
TEST_BIN = unit_tests
OPT_BIN = optimize_threshold

.PHONY: all clean test optimize

all: $(APP)

$(APP): test main.c hash_table.c hash_table.h
	$(CC) $(CFLAGS) main.c hash_table.c -o $(APP).out

$(TEST_BIN): unit_test.c hash_table.c hash_table.h
	$(CC) $(CFLAGS) unit_test.c hash_table.c -o $(TEST_BIN).out

test: $(TEST_BIN)
	./$(TEST_BIN).out

$(OPT_BIN): optimize_threshold.c hash_table.c hash_table.h
	$(CC) $(CFLAGS) optimize_threshold.c hash_table.c -o $(OPT_BIN).out

optimize: $(OPT_BIN)
	./$(OPT_BIN).out

clean:
	rm -f $(APP).out $(TEST_BIN).out $(OPT_BIN).out
