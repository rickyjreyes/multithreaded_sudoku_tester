#include "sudoku.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int box_width(int size) {
    if (size == 4) return 2;
    if (size == 9) return 3;
    if (size == 16) return 4;
    return 0;
}

int sudoku_load(FILE *stream, int size, sudoku_board *board,
                char *error, size_t error_length) {
    int ch, line = 1, in_comment = 0;
    size_t count = 0;
    int box = box_width(size);
    if (!stream || !board || !error || error_length == 0) return -1;
    if (!box) {
        snprintf(error, error_length, "size must be 4, 9 or 16");
        return -1;
    }
    memset(board, 0, sizeof(*board));
    board->size = size;
    board->box = box;
    while ((ch = fgetc(stream)) != EOF) {
        int value;
        if (ch == '\n') {
            ++line;
            in_comment = 0;
            continue;
        }
        if (in_comment) continue;
        if (ch == '#') {
            in_comment = 1;
            continue;
        }
        if (isspace((unsigned char)ch) || ch == '|' || ch == '+' ||
            ch == '-' || ch == ',' || ch == ':') continue;
        if (ch == '.' || ch == '0') value = 0;
        else if (ch >= '1' && ch <= '9') value = ch - '0';
        else if (ch >= 'A' && ch <= 'G') value = ch - 'A' + 10;
        else if (ch >= 'a' && ch <= 'g') value = ch - 'a' + 10;
        else {
            snprintf(error, error_length, "unexpected character at line %d", line);
            return -1;
        }
        if (value > size) {
            snprintf(error, error_length, "value out of range at line %d", line);
            return -1;
        }
        if (count >= (size_t)size * (size_t)size) {
            snprintf(error, error_length, "too many cells (expected %d)", size * size);
            return -1;
        }
        board->cells[count / (size_t)size][count % (size_t)size] = (unsigned char)value;
        ++count;
    }
    if (ferror(stream)) {
        snprintf(error, error_length, "failed while reading input");
        return -1;
    }
    if (count != (size_t)size * (size_t)size) {
        snprintf(error, error_length, "expected %d cells, received %zu", size * size, count);
        return -1;
    }
    error[0] = '\0';
    return 0;
}

void sudoku_print(FILE *stream, const sudoku_board *board) {
    static const char digits[] = ".123456789ABCDEFG";
    if (!stream || !board) return;
    for (int r = 0; r < board->size; ++r) {
        for (int c = 0; c < board->size; ++c) {
            unsigned value = board->cells[r][c];
            fputc(value <= 16 ? digits[value] : '?', stream);
            if (c + 1 < board->size) fputc(' ', stream);
            if ((c + 1) % board->box == 0 && c + 1 < board->size) fputs(" | ", stream);
        }
        fputc('\n', stream);
        if ((r + 1) % board->box == 0 && r + 1 < board->size) fputc('\n', stream);
    }
}

void sudoku_demo(sudoku_board *board) {
    static const unsigned char original[9][9] = {
        {6,5,3,1,2,8,7,9,4}, {1,7,4,3,5,9,6,8,2},
        {9,2,8,4,6,7,5,3,1}, {2,8,6,5,1,4,3,7,9},
        {3,9,1,7,8,2,4,5,6}, {5,4,7,6,9,3,2,1,8},
        {8,6,5,2,3,1,9,4,7}, {4,1,2,9,7,5,8,6,3},
        {7,3,9,8,4,6,1,2,5}
    };
    memset(board, 0, sizeof(*board));
    board->size = 9;
    board->box = 3;
    for (int r = 0; r < 9; ++r)
        for (int c = 0; c < 9; ++c) board->cells[r][c] = original[r][c];
}
