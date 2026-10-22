#include "validator.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool sudoku_unit_valid(const sudoku_board *b, sudoku_unit unit, int index) {
    uint32_t bits = 0;
    int n, box;
    if (!b || (n = b->size) < 1 || n > SUDOKU_MAX ||
        (box = b->box) < 1 || box * box != n ||
        index < 0 || index >= n || unit < UNIT_ROW || unit > UNIT_BOX) return false;
    for (int i = 0; i < n; ++i) {
        int r = 0, c = 0;
        if (unit == UNIT_ROW) { r = index; c = i; }
        else if (unit == UNIT_COLUMN) { r = i; c = index; }
        else {
            r = (index / box) * box + i / box;
            c = (index % box) * box + i % box;
        }
        unsigned value = b->cells[r][c];
        if (value < 1 || value > (unsigned)n) return false;
        uint32_t bit = UINT32_C(1) << (value - 1);
        if (bits & bit) return false;
        bits |= bit;
    }
    return bits == ((UINT32_C(1) << n) - 1);
}

typedef struct {
    const sudoku_board *board;
    sudoku_report *report;
    sudoku_unit unit;
    int start;
    int count;
} task;

static void *run_task(void *argument) {
    task *t = argument;
    bool *out = t->unit == UNIT_ROW ? t->report->rows :
                t->unit == UNIT_COLUMN ? t->report->columns : t->report->boxes;
    for (int i = t->start; i < t->start + t->count; ++i)
        out[i] = sudoku_unit_valid(t->board, t->unit, i);
    return NULL;
}

typedef struct {
    task *tasks;
    int count;
    int next;
    pthread_mutex_t mutex;
} pool_state;

static void *run_pool(void *argument) {
    pool_state *state = argument;
    for (;;) {
        int index;
        pthread_mutex_lock(&state->mutex);
        index = state->next++;
        pthread_mutex_unlock(&state->mutex);
        if (index >= state->count) break;
        run_task(&state->tasks[index]);
    }
    return NULL;
}

static int start_threads(task *tasks, int count) {
    pthread_t threads[3 * SUDOKU_MAX];
    int started = 0, ok = 1;
    for (int i = 0; i < count; ++i) {
        if (pthread_create(&threads[i], NULL, run_task, &tasks[i]) != 0) {
            ok = 0;
            break;
        }
        ++started;
    }
    for (int i = 0; i < started; ++i)
        if (pthread_join(threads[i], NULL) != 0) ok = 0;
    return ok ? 0 : -1;
}

static int start_pool(task *tasks, int count, unsigned workers) {
    pool_state state = { .tasks = tasks, .count = count, .next = 0 };
    pthread_t threads[256];
    int started = 0, ok = 1;
    if (workers < 1 || workers > 256) return -1;
    if (pthread_mutex_init(&state.mutex, NULL) != 0) return -1;
    for (unsigned i = 0; i < workers; ++i) {
        if (pthread_create(&threads[i], NULL, run_pool, &state) != 0) {
            ok = 0;
            break;
        }
        ++started;
    }
    for (int i = 0; i < started; ++i)
        if (pthread_join(threads[i], NULL) != 0) ok = 0;
    pthread_mutex_destroy(&state.mutex);
    return ok ? 0 : -1;
}

int sudoku_validate(const sudoku_board *board, sudoku_mode mode,
                    unsigned workers, sudoku_report *report) {
    task tasks[3 * SUDOKU_MAX];
    int count = 0, n;
    if (!board || !report || (n = board->size) < 1 || n > SUDOKU_MAX ||
        board->box < 1 || board->box * board->box != n ||
        mode < MODE_SEQUENTIAL || mode > MODE_POOL) return -1;
    memset(report, 0, sizeof(*report));
    report->complete = true;
    if (mode == MODE_ORIGINAL) {
        tasks[count++] = (task){board, report, UNIT_ROW, 0, n};
        tasks[count++] = (task){board, report, UNIT_COLUMN, 0, n};
        for (int i = 0; i < n; ++i)
            tasks[count++] = (task){board, report, UNIT_BOX, i, 1};
    } else {
        for (int kind = UNIT_ROW; kind <= UNIT_BOX; ++kind)
            for (int i = 0; i < n; ++i)
                tasks[count++] = (task){board, report, (sudoku_unit)kind, i, 1};
    }
    if (mode == MODE_SEQUENTIAL) {
        for (int i = 0; i < count; ++i) run_task(&tasks[i]);
    } else if (mode == MODE_POOL) {
        if (start_pool(tasks, count, workers) != 0) return -1;
    } else if (start_threads(tasks, count) != 0) return -1;

    for (int i = 0; i < n; ++i) {
        report->failures += !report->rows[i];
        report->failures += !report->columns[i];
        report->failures += !report->boxes[i];
        for (int j = 0; j < n; ++j)
            if (board->cells[i][j] == 0) report->complete = false;
    }
    report->valid = report->complete && report->failures == 0;
    return 0;
}

const char *sudoku_mode_name(sudoku_mode mode) {
    switch (mode) {
        case MODE_SEQUENTIAL: return "sequential";
        case MODE_ORIGINAL: return "11-thread";
        case MODE_FINE: return "27-thread";
        case MODE_POOL: return "pool";
        default: return "unknown";
    }
}
