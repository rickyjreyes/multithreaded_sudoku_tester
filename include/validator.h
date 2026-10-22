#ifndef VALIDATOR_H
#define VALIDATOR_H

#include "sudoku.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum { UNIT_ROW, UNIT_COLUMN, UNIT_BOX } sudoku_unit;
typedef enum { MODE_SEQUENTIAL, MODE_ORIGINAL, MODE_FINE, MODE_POOL } sudoku_mode;

typedef struct {
    bool rows[SUDOKU_MAX];
    bool columns[SUDOKU_MAX];
    bool boxes[SUDOKU_MAX];
    bool complete;
    bool valid;
    size_t failures;
} sudoku_report;

/* Results are per-constraint, with the same semantics for every mode.
 * ORIGINAL = one row worker, one column worker, N box workers (11 on 9x9).
 * FINE = 3*N workers (27 on 9x9); POOL = bounded reusable workers per call.
 * Returns -1 only for invalid configuration or threading/resource failure. */
int sudoku_validate(const sudoku_board *board, sudoku_mode mode,
                    unsigned workers, sudoku_report *report);
bool sudoku_unit_valid(const sudoku_board *board, sudoku_unit unit, int index);
const char *sudoku_mode_name(sudoku_mode mode);

#endif
