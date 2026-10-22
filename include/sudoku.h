#ifndef SUDOKU_H
#define SUDOKU_H

#include <stddef.h>
#include <stdio.h>

#define SUDOKU_MAX 16

typedef struct {
    int size;
    int box;
    unsigned char cells[SUDOKU_MAX][SUDOKU_MAX];
} sudoku_board;

/* Accepts sizes 4, 9, 16. Digits 1-9, A-G (10-16), '.' or '0' for blanks.
 * Separators: whitespace, '|', '+', '-', ',' and ':'; '#' starts a comment.
 * Returns 0 on success or -1 with a human-readable error. */
int sudoku_load(FILE *stream, int size, sudoku_board *board,
                char *error, size_t error_length);
void sudoku_print(FILE *stream, const sudoku_board *board);
void sudoku_demo(sudoku_board *board);

#endif
