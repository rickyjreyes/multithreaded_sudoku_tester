CC ?= cc
CPPFLAGS ?= -Iinclude -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS ?= -pthread

CORE = src/sudoku.c src/validator.c
BIN = sudoku-tester
TESTBIN = build/test_suite
LABCORE = lab/lab.c
LABBIN = concurrency-lab
LABTEST = build/test_lab

.PHONY: all test clean sanitize tsan
all: $(BIN) $(LABBIN)

$(BIN): $(CORE) src/main.c include/sudoku.h include/validator.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(LDFLAGS) -o $@ $(CORE) src/main.c $(LDLIBS)

$(TESTBIN): $(CORE) tests/test_suite.c include/sudoku.h include/validator.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(LDFLAGS) -o $@ $(CORE) tests/test_suite.c $(LDLIBS)

$(LABBIN): $(LABCORE) lab/main.c lab/lab.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Ilab -pthread $(LDFLAGS) -o $@ $(LABCORE) lab/main.c $(LDLIBS)

$(LABTEST): $(LABCORE) tests/test_lab.c lab/lab.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Ilab -pthread $(LDFLAGS) -o $@ $(LABCORE) tests/test_lab.c $(LDLIBS)

test: $(TESTBIN) $(LABTEST)
	./$(TESTBIN)
	./$(LABTEST)

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=address,undefined" LDFLAGS="-fsanitize=address,undefined" all test

tsan:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O1 -g -std=c11 -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=thread" LDFLAGS="-fsanitize=thread" all test

clean:
	rm -rf $(BIN) $(LABBIN) build
