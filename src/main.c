#include "sudoku.h"
#include "validator.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *path;
    const char *format;
    sudoku_mode mode;
    int size;
    unsigned workers;
    unsigned iterations;
} options;

static void usage(FILE *out) {
    fputs(
        "Usage:\n"
        "  sudoku-tester validate FILE [--size 4|9|16] [--mode sequential|11|27|pool]\n"
        "                              [--threads N] [--format text|json]\n"
        "  sudoku-tester benchmark [FILE] [--size 4|9|16] [--threads N]\n"
        "                               [--iterations N] [--format text|csv]\n"
        "  sudoku-tester batch DIRECTORY [--size 4|9|16] [--mode sequential|11|27|pool]\n"
        "                                [--threads N]\n"
        "  sudoku-tester --help\n"
        "FILE: one cell per symbol, digits 1-9, A-G for 10-16; '.' or '0' is blank.\n"
        "No FILE for benchmark uses a generated complete board (2017 sample for 9x9).\n", out);
}

static int number(const char *value, unsigned maximum, unsigned *out) {
    char *end = NULL;
    unsigned long parsed;
    if (!value || !*value || *value == '-') return -1;
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno || !end || *end || parsed < 1 || parsed > maximum) return -1;
    *out = (unsigned)parsed;
    return 0;
}

static int parse_options(int argc, char **argv, options *o) {
    o->path = NULL;
    o->format = "text";
    o->mode = MODE_ORIGINAL;
    o->size = 9;
    o->workers = 4;
    o->iterations = 1000;
    for (int i = 2; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--size") || !strcmp(a, "--threads") ||
            !strcmp(a, "--iterations") || !strcmp(a, "--mode") ||
            !strcmp(a, "--format")) {
            if (++i >= argc) return -1;
            const char *v = argv[i];
            unsigned parsed;
            if (!strcmp(a, "--size")) {
                if (number(v, 16, &parsed) || (parsed != 4 && parsed != 9 && parsed != 16))
                    return -1;
                o->size = (int)parsed;
            } else if (!strcmp(a, "--threads")) {
                if (number(v, 256, &o->workers)) return -1;
            } else if (!strcmp(a, "--iterations")) {
                if (number(v, 1000000, &o->iterations)) return -1;
            } else if (!strcmp(a, "--mode")) {
                if (!strcmp(v, "sequential")) o->mode = MODE_SEQUENTIAL;
                else if (!strcmp(v, "11") || !strcmp(v, "original")) o->mode = MODE_ORIGINAL;
                else if (!strcmp(v, "27") || !strcmp(v, "fine")) o->mode = MODE_FINE;
                else if (!strcmp(v, "pool")) o->mode = MODE_POOL;
                else return -1;
            } else {
                if (strcmp(v, "text") && strcmp(v, "json") && strcmp(v, "csv"))
                    return -1;
                o->format = v;
            }
        } else if (a[0] == '-' || o->path) return -1;
        else o->path = a;
    }
    return 0;
}

static int load_path(const char *path, int size, sudoku_board *board) {
    char error[160];
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return -1;
    }
    int status = sudoku_load(file, size, board, error, sizeof(error));
    if (fclose(file) != 0) status = -1;
    if (status != 0) fprintf(stderr, "%s: %s\n", path, error[0] ? error : "I/O failure");
    return status;
}

static const char *status_name(const sudoku_report *r) {
    return r->valid ? "VALID" : !r->complete ? "INCOMPLETE" : "INVALID";
}

static void print_json(const sudoku_board *b, const sudoku_report *r) {
    printf("{\"size\":%d,\"status\":\"%s\",\"valid\":%s,\"complete\":%s,"
           "\"failures\":%zu,\"invalid_rows\":[",
           b->size, status_name(r), r->valid ? "true" : "false",
           r->complete ? "true" : "false", r->failures);
    bool first = true;
    for (int i = 0; i < b->size; ++i) if (!r->rows[i]) {
        printf("%s%d", first ? "" : ",", i + 1);
        first = false;
    }
    fputs("],\"invalid_columns\":[", stdout);
    first = true;
    for (int i = 0; i < b->size; ++i) if (!r->columns[i]) {
        printf("%s%d", first ? "" : ",", i + 1);
        first = false;
    }
    fputs("],\"invalid_boxes\":[", stdout);
    first = true;
    for (int i = 0; i < b->size; ++i) if (!r->boxes[i]) {
        printf("%s%d", first ? "" : ",", i + 1);
        first = false;
    }
    puts("]}");
}

static void print_text(const sudoku_board *b, const sudoku_report *r) {
    sudoku_print(stdout, b);
    printf("\nResult: %s (%zu failing constraints)\n", status_name(r), r->failures);
    for (int i = 0; i < b->size; ++i) {
        if (!r->rows[i]) printf("  Row %d: FAIL\n", i + 1);
        if (!r->columns[i]) printf("  Column %d: FAIL\n", i + 1);
        if (!r->boxes[i]) printf("  Box %d: FAIL\n", i + 1);
    }
}

static double seconds_between(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec - a.tv_sec) +
           (double)(b.tv_nsec - a.tv_nsec) / 1000000000.0;
}

static bool same_report(const sudoku_report *a, const sudoku_report *b, int n) {
    return a->valid == b->valid && a->complete == b->complete &&
           a->failures == b->failures &&
           memcmp(a->rows, b->rows, (size_t)n * sizeof(bool)) == 0 &&
           memcmp(a->columns, b->columns, (size_t)n * sizeof(bool)) == 0 &&
           memcmp(a->boxes, b->boxes, (size_t)n * sizeof(bool)) == 0;
}

static int benchmark(const options *o) {
    sudoku_board board;
    sudoku_report baseline;
    double baseline_time = 0.0;
    sudoku_mode modes[] = {MODE_SEQUENTIAL, MODE_ORIGINAL, MODE_FINE, MODE_POOL};
    if (o->path) {
        if (load_path(o->path, o->size, &board)) return 2;
    } else if (o->size == 9) sudoku_demo(&board);
    else {
        memset(&board, 0, sizeof(board));
        board.size = o->size;
        board.box = o->size == 4 ? 2 : 4;
        for (int r = 0; r < board.size; ++r)
            for (int c = 0; c < board.size; ++c)
                board.cells[r][c] = (unsigned char)
                    ((r * board.box + r / board.box + c) % board.size + 1);
    }
    if (sudoku_validate(&board, MODE_SEQUENTIAL, o->workers, &baseline)) return 2;
    bool csv = !strcmp(o->format, "csv");
    if (csv) puts("mode,iterations,elapsed_ms,mean_us,boards_per_second,speedup");
    else printf("Benchmark: %dx%d, %u iterations, baseline board %s\n",
                board.size, board.size, o->iterations, status_name(&baseline));
    for (size_t mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode) {
        struct timespec a, b;
        sudoku_report result;
        if (clock_gettime(CLOCK_MONOTONIC, &a) != 0) return 2;
        for (unsigned i = 0; i < o->iterations; ++i) {
            if (sudoku_validate(&board, modes[mode], o->workers, &result) != 0) {
                fputs("thread creation or synchronization failed\n", stderr);
                return 2;
            }
            if (!same_report(&baseline, &result, board.size)) {
                fputs("ERROR: validator modes disagree\n", stderr);
                return 2;
            }
        }
        if (clock_gettime(CLOCK_MONOTONIC, &b) != 0) return 2;
        double elapsed = seconds_between(a, b);
        if (mode == 0) baseline_time = elapsed;
        double speedup = elapsed > 0.0 ? baseline_time / elapsed : 0.0;
        double rate = elapsed > 0.0 ? o->iterations / elapsed : 0.0;
        if (csv) printf("%s,%u,%.6f,%.6f,%.3f,%.4f\n",
                        sudoku_mode_name(modes[mode]), o->iterations,
                        elapsed * 1000.0, elapsed * 1000000.0 / o->iterations,
                        rate, speedup);
        else printf("%-12s %10.3f ms  %10.3f us/board  %12.0f boards/s  %7.3fx\n",
                    sudoku_mode_name(modes[mode]), elapsed * 1000.0,
                    elapsed * 1000000.0 / o->iterations, rate, speedup);
    }
    return 0;
}

static int batch(const options *o) {
    DIR *dir = opendir(o->path);
    if (!dir) {
        fprintf(stderr, "%s: %s\n", o->path, strerror(errno));
        return 2;
    }
    struct dirent *entry;
    size_t checked = 0, invalid = 0, errors = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t n = strlen(entry->d_name);
        if (n < 5 || strcmp(entry->d_name + n - 4, ".sdk")) continue;
        size_t len = strlen(o->path) + 1 + n + 1;
        char *full = malloc(len);
        if (!full) { ++errors; break; }
        snprintf(full, len, "%s/%s", o->path, entry->d_name);
        sudoku_board board;
        sudoku_report report;
        if (load_path(full, o->size, &board) ||
            sudoku_validate(&board, o->mode, o->workers, &report)) {
            ++errors;
        } else {
            ++checked;
            if (!report.valid) ++invalid;
            printf("%s: %s (%zu failing constraints)\n",
                   entry->d_name, status_name(&report), report.failures);
        }
        free(full);
    }
    if (closedir(dir) != 0) ++errors;
    printf("Batch: %zu checked, %zu invalid/incomplete, %zu errors\n",
           checked, invalid, errors);
    if (errors || checked == 0) return 2;
    return invalid ? 1 : 0;
}

int main(int argc, char **argv) {
    options o;
    if (argc < 2 || (argc == 2 && !strcmp(argv[1], "--help"))) {
        usage(argc < 2 ? stderr : stdout);
        return argc < 2 ? 2 : 0;
    }
    if (parse_options(argc, argv, &o)) {
        usage(stderr);
        return 2;
    }
    if (!strcmp(argv[1], "validate")) {
        sudoku_board board;
        sudoku_report report;
        if (!o.path || !strcmp(o.format, "csv")) { usage(stderr); return 2; }
        if (load_path(o.path, o.size, &board)) return 2;
        if (sudoku_validate(&board, o.mode, o.workers, &report)) {
            fputs("validation failed: thread/resource error\n", stderr);
            return 2;
        }
        if (!strcmp(o.format, "json")) print_json(&board, &report);
        else print_text(&board, &report);
        return report.valid ? 0 : 1;
    }
    if (!strcmp(argv[1], "benchmark")) {
        if (!strcmp(o.format, "json")) { usage(stderr); return 2; }
        return benchmark(&o);
    }
    if (!strcmp(argv[1], "batch")) {
        if (!o.path || strcmp(o.format, "text")) { usage(stderr); return 2; }
        return batch(&o);
    }
    usage(stderr);
    return 2;
}
