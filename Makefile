CC ?= cc
CPPFLAGS ?= -Iinclude -Iwarehouse -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS ?= -pthread

CORE = src/sudoku.c src/validator.c
BIN = sudoku-tester
TESTBIN = build/test_suite
HIVE = warehouse/hive.c
HIVEBIN = warehouse-sim
HIVETEST = build/test_warehouse

.PHONY: all test clean sanitize tsan
all: $(BIN) $(HIVEBIN)

$(BIN): $(CORE) src/main.c include/sudoku.h include/validator.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(LDFLAGS) -o $@ $(CORE) src/main.c $(LDLIBS)

$(TESTBIN): $(CORE) tests/test_suite.c include/sudoku.h include/validator.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(LDFLAGS) -o $@ $(CORE) tests/test_suite.c $(LDLIBS)

$(HIVEBIN): $(HIVE) warehouse/main.c warehouse/hive.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(LDFLAGS) -o $@ $(HIVE) warehouse/main.c $(LDLIBS)

$(HIVETEST): $(HIVE) tests/test_warehouse.c warehouse/hive.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(LDFLAGS) -o $@ $(HIVE) tests/test_warehouse.c $(LDLIBS)

test: $(TESTBIN) $(HIVETEST)
	./$(TESTBIN)
	./$(HIVETEST)

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined" all test

tsan:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=thread" LDFLAGS="-fsanitize=thread" all test

clean:
	rm -rf $(BIN) $(HIVEBIN) build
