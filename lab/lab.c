#include "lab.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static uint64_t mix64(uint64_t x) {
    x ^= x >> 30;
    x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27;
    x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}

static uint64_t calculate(const lab_config *c, size_t index) {
    uint64_t x = mix64(c->seed ^ (UINT64_C(0x9e3779b97f4a7c15) * (index + 1u)));
    /* Every eighth synthetic job has 8x the arithmetic in skewed mode.
     * There is no I/O, position, route or operational workload model. */
    unsigned factor = (c->profile == LAB_SKEWED && index % 8u == 0u) ? 8u : 1u;
    unsigned count = c->work * factor; /* bounded: at most 800,000 */
    for (unsigned i = 0; i < count; ++i)
        x = mix64(x + UINT64_C(0x9e3779b97f4a7c15) + (uint64_t)i);
    return x;
}

typedef struct {
    const lab_config *config;
    uint64_t *results;
    lab_mode mode;
    size_t mutex_next;
    atomic_size_t atomic_next;
    atomic_int error;
    pthread_mutex_t lock;
} shared;

typedef struct {
    shared *state;
    size_t start;
    size_t end;
} worker_arg;

static void *worker(void *argument) {
    worker_arg *arg = argument;
    shared *state = arg->state;
    if (state->mode == LAB_STATIC) {
        for (size_t i = arg->start; i < arg->end; ++i)
            state->results[i] = calculate(state->config, i);
        return NULL;
    }
    for (;;) {
        size_t index;
        if (state->mode == LAB_ATOMIC) {
            index = atomic_fetch_add_explicit(&state->atomic_next, 1u,
                                               memory_order_relaxed);
        } else {
            if (pthread_mutex_lock(&state->lock) != 0) {
                atomic_store(&state->error, 1);
                break;
            }
            index = state->mutex_next++;
            if (pthread_mutex_unlock(&state->lock) != 0) {
                atomic_store(&state->error, 1);
                break;
            }
        }
        if (index >= state->config->jobs) break;
        state->results[index] = calculate(state->config, index);
    }
    return NULL;
}

const char *lab_mode_name(lab_mode mode) {
    switch (mode) {
        case LAB_SEQUENTIAL: return "sequential";
        case LAB_STATIC: return "static";
        case LAB_ATOMIC: return "atomic";
        case LAB_MUTEX: return "mutex";
        default: return "unknown";
    }
}

const char *lab_profile_name(lab_profile profile) {
    switch (profile) {
        case LAB_UNIFORM: return "uniform";
        case LAB_SKEWED: return "skewed";
        default: return "unknown";
    }
}

int lab_run(const lab_config *c, lab_mode mode,
            uint64_t *results, uint64_t *digest) {
    if (!c || !results || !digest || c->jobs == 0 ||
        c->jobs > LAB_MAX_JOBS || c->threads == 0 ||
        c->threads > LAB_MAX_THREADS || c->work == 0 ||
        c->work > LAB_MAX_WORK ||
        (c->profile != LAB_UNIFORM && c->profile != LAB_SKEWED) ||
        mode < LAB_SEQUENTIAL || mode > LAB_MUTEX) return -1;

    if (mode == LAB_SEQUENTIAL) {
        for (size_t i = 0; i < c->jobs; ++i)
            results[i] = calculate(c, i);
    } else {
        shared s;
        pthread_t ids[LAB_MAX_THREADS];
        worker_arg args[LAB_MAX_THREADS];
        unsigned started = 0;
        int failed = 0;
        memset(&s, 0, sizeof(s));
        s.config = c;
        s.results = results;
        s.mode = mode;
        atomic_init(&s.atomic_next, 0u);
        atomic_init(&s.error, 0);
        bool has_mutex = mode == LAB_MUTEX;
        if (has_mutex && pthread_mutex_init(&s.lock, NULL) != 0) return -1;

        for (unsigned i = 0; i < c->threads; ++i) {
            args[i] = (worker_arg){
                .state = &s,
                .start = (c->jobs * i) / c->threads,
                .end = (c->jobs * (i + 1u)) / c->threads
            };
            if (pthread_create(&ids[i], NULL, worker, &args[i]) != 0) {
                failed = 1;
                break;
            }
            ++started;
        }
        for (unsigned i = 0; i < started; ++i)
            if (pthread_join(ids[i], NULL) != 0) failed = 1;
        if (has_mutex && pthread_mutex_destroy(&s.lock) != 0) failed = 1;
        if (atomic_load(&s.error)) failed = 1;
        if (failed) return -1;
    }
    /* Stable reduction independent of worker completion order. */
    uint64_t d = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < c->jobs; ++i)
        d = mix64(d ^ results[i] ^ (uint64_t)i);
    *digest = d;
    return 0;
}
