# Multithreaded Sudoku Tester (C / pthreads)

A 2017 C coursework exercise exploring concurrent validation of a hard-coded 9×9 Sudoku grid using POSIX threads. Its intended design uses **11 worker threads**: one to check all rows, one for all columns, and nine for the 3×3 subgrids.

## Source and intended operation

[`sudoku_rickyreyes_794.c`](sudoku_rickyreyes_794.c) embeds the example grid directly in `main`, starts the validator threads with `pthread_create`, joins them, prints the grid, and reports whether the puzzle was accepted. There is no file-input interface and no solving/backtracking algorithm: it attempts to **validate an already completed grid**.

## Build

On a POSIX environment with a C compiler and pthread support:

```sh
gcc -std=c11 -Wall -Wextra -pthread sudoku_rickyreyes_794.c -o sudoku_tester
./sudoku_tester
```

This command describes the intended compilation; running the current source is **not recommended until the defects below are fixed**.

## Known correctness and safety problems

- `initialize_row_column_square` loops with `i <= GRID_SIZE` although `data` has only nine elements, writing `data[9]` out of bounds.
- The subgrid coordinate mapping indexes squares as though entries ran from 1 through 9, while the spawned threads use `data[0]` through `data[8]`; one intended subgrid is omitted.
- `isSolved` sets the result to true if **any** square passes alongside row/column results, instead of requiring **all nine** squares to pass.
- Values are used to index a 10-element `check_*` array without checking they lie from 1 to 9.
- Heap-allocated thread parameters are not freed; thread-creation failure is not checked.
- Boolean outcomes are passed through `void *` thread-return values, which is not a robust portable result representation.

Accordingly, an apparent “CORRECT” message is not reliable evidence that the grid is valid. The file is useful as a concurrency/validation learning example and a starting point for correcting these defects, not as a trusted Sudoku checker.

## Status

Historical single-file exercise. No standalone tests, general-purpose input parser or explicit license are included.
