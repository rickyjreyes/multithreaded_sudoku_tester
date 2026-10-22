#include "sudoku.h"
#include "validator.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static sudoku_board generated(int n) {
    sudoku_board board = {0};
    board.size = n;
    board.box = n == 4 ? 2 : n == 9 ? 3 : 4;
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c)
            board.cells[r][c] =
                (unsigned char)((r * board.box + r / board.box + c) % n + 1);
    return board;
}

static void compare_modes(const sudoku_board *board, bool expected_valid) {
    sudoku_report baseline, current;
    assert(sudoku_validate(board, MODE_SEQUENTIAL, 1, &baseline) == 0);
    assert(baseline.valid == expected_valid);
    const sudoku_mode modes[] = {MODE_ORIGINAL, MODE_FINE, MODE_POOL};
    for (size_t m = 0; m < sizeof(modes) / sizeof(modes[0]); ++m) {
        for (unsigned workers = 1; workers <= 4; workers += 3) {
            assert(sudoku_validate(board, modes[m], workers, &current) == 0);
            assert(current.valid == baseline.valid);
            assert(current.complete == baseline.complete);
            assert(current.failures == baseline.failures);
            for (int i = 0; i < board->size; ++i) {
                assert(current.rows[i] == baseline.rows[i]);
                assert(current.columns[i] == baseline.columns[i]);
                assert(current.boxes[i] == baseline.boxes[i]);
            }
        }
    }
}

static void test_sizes_and_corruption(void) {
    const int sizes[] = {4, 9, 16};
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); ++s) {
        sudoku_board board = generated(sizes[s]);
        compare_modes(&board, true);
        board.cells[0][0] = 0;
        compare_modes(&board, false);
        sudoku_report result;
        assert(sudoku_validate(&board, MODE_SEQUENTIAL, 1, &result) == 0);
        assert(!result.complete);
        board = generated(sizes[s]);
        board.cells[0][0] = (unsigned char)(board.size + 1);
        compare_modes(&board, false);
        board = generated(sizes[s]);
        board.cells[0][0] = board.cells[0][1];
        compare_modes(&board, false);
        assert(!sudoku_unit_valid(&board, UNIT_ROW, 0));
        assert(!sudoku_unit_valid(&board, UNIT_BOX, 0));
    }
}

static void test_isolated_box_failures(void) {
    sudoku_board board = generated(9);
    for (int r = 0; r < 9; ++r)
        for (int c = 0; c < 9; ++c)
            board.cells[r][c] = (unsigned char)((r + c) % 9 + 1);
    sudoku_report report;
    assert(sudoku_validate(&board, MODE_SEQUENTIAL, 1, &report) == 0);
    assert(!report.valid);
    assert(report.complete);
    assert(report.failures == 9);
    for (int i = 0; i < 9; ++i) {
        assert(report.rows[i]);
        assert(report.columns[i]);
        assert(!report.boxes[i]);
    }
    compare_modes(&board, false);
}

static int load_text(const char *source, int n, sudoku_board *board) {
    FILE *file = tmpfile();
    assert(file);
    assert(fputs(source, file) >= 0);
    rewind(file);
    char error[128];
    int status = sudoku_load(file, n, board, error, sizeof(error));
    assert(fclose(file) == 0);
    return status;
}

static void test_parser(void) {
    sudoku_board board;
    assert(load_text("# valid 4x4\n12|34\n34|12\n21|43\n43|21\n", 4, &board) == 0);
    compare_modes(&board, true);
    assert(load_text("1234\n3412\n2143\n4321\n", 4, &board) == 0);
    assert(load_text("1234\n3412\n2143\n432H\n", 4, &board) != 0);
    assert(load_text("1234\n3412\n2143\n432\n", 4, &board) != 0);
    assert(load_text("1234\n3412\n2143\n43211\n", 4, &board) != 0);
    assert(load_text("0234\n3412\n2143\n4321\n", 4, &board) == 0);
    sudoku_report report;
    assert(sudoku_validate(&board, MODE_SEQUENTIAL, 1, &report) == 0);
    assert(!report.complete);
    assert(load_text("1234\n3412\n2143\n4321\n", 6, &board) != 0);
    sudoku_board source = generated(16);
    FILE *file = tmpfile();
    assert(file);
    for (int r = 0; r < 16; ++r) {
        for (int c = 0; c < 16; ++c) {
            int value = source.cells[r][c];
            assert(fputc(value <= 9 ? '0' + value : 'A' + value - 10, file) != EOF);
        }
        assert(fputc('\n', file) != EOF);
    }
    rewind(file);
    char error[128];
    assert(sudoku_load(file, 16, &board, error, sizeof(error)) == 0);
    assert(fclose(file) == 0);
    compare_modes(&board, true);
}

static void test_api_errors(void) {
    sudoku_board board = generated(9);
    sudoku_report result;
    assert(sudoku_validate(NULL, MODE_SEQUENTIAL, 1, &result) != 0);
    assert(sudoku_validate(&board, MODE_SEQUENTIAL, 1, NULL) != 0);
    assert(sudoku_validate(&board, MODE_POOL, 0, &result) != 0);
    assert(sudoku_validate(&board, MODE_POOL, 257, &result) != 0);
    assert(sudoku_validate(&board, (sudoku_mode)99, 1, &result) != 0);
    assert(!sudoku_unit_valid(&board, UNIT_BOX, 9));
    assert(!sudoku_unit_valid(&board, UNIT_ROW, -1));
    sudoku_demo(&board);
    compare_modes(&board, true);
}

int main(void) {
    test_sizes_and_corruption();
    test_isolated_box_failures();
    test_parser();
    test_api_errors();
    puts("All Sudoku validator tests passed.");
    return 0;
}
