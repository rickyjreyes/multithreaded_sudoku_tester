#include "lab.h"
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    lab_config config;
    lab_mode mode;
    unsigned trials;
} options;

static void usage(FILE *out) {
    fputs(
        "Public Concurrency Lab — synthetic CPU work only\n"
        "Usage:\n"
        "  concurrency-lab run [--mode sequential|static|atomic|mutex] [options]\n"
        "  concurrency-lab compare [options]\n"
        "  concurrency-lab benchmark [options]\n"
        "Options:\n"
        "  --jobs N         independent tasks, 1..65536 (default 512)\n"
        "  --work N         arithmetic rounds, 1..100000 (default 500)\n"
        "  --threads N      worker count, 1..32 (default 4)\n"
        "  --seed N         unsigned 64-bit seed (default 1)\n"
        "  --profile uniform|skewed (default uniform)\n"
        "  --trials N       benchmark trials, 1..100 (default 5)\n"
        "  --mode NAME      run command only (default sequential)\n"
        "  --help\n", out);
}

static int parse_uint(const char *s, uint64_t min, uint64_t max, uint64_t *out) {
    char *end = NULL;
    unsigned long long n;
    if (!s || !*s || *s == '-' || *s == '+') return -1;
    errno = 0;
    n = strtoull(s, &end, 10);
    if (errno || !end || *end || n < min || n > max) return -1;
    *out = (uint64_t)n;
    return 0;
}

static int parse(int argc, char **argv, options *o) {
    *o = (options){
        .config = { .jobs=512, .work=500, .threads=4,
                    .seed=1, .profile=LAB_UNIFORM },
        .mode = LAB_SEQUENTIAL, .trials = 5
    };
    bool explicit_mode = false, explicit_trials = false;
    for (int i = 2; i < argc; ++i) {
        const char *key = argv[i];
        if (!strcmp(key, "--help")) return 1;
        if (++i >= argc) return -1;
        const char *v = argv[i];
        uint64_t value = 0;
        if (!strcmp(key, "--jobs")) {
            if (parse_uint(v, 1, LAB_MAX_JOBS, &value)) return -1;
            o->config.jobs = (size_t)value;
        } else if (!strcmp(key, "--work")) {
            if (parse_uint(v, 1, LAB_MAX_WORK, &value)) return -1;
            o->config.work = (unsigned)value;
        } else if (!strcmp(key, "--threads")) {
            if (parse_uint(v, 1, LAB_MAX_THREADS, &value)) return -1;
            o->config.threads = (unsigned)value;
        } else if (!strcmp(key, "--seed")) {
            if (parse_uint(v, 0, UINT64_MAX, &value)) return -1;
            o->config.seed = value;
        } else if (!strcmp(key, "--trials")) {
            if (parse_uint(v, 1, 100, &value)) return -1;
            o->trials = (unsigned)value;
            explicit_trials = true;
        } else if (!strcmp(key, "--profile")) {
            if (!strcmp(v, "uniform")) o->config.profile = LAB_UNIFORM;
            else if (!strcmp(v, "skewed")) o->config.profile = LAB_SKEWED;
            else return -1;
        } else if (!strcmp(key, "--mode")) {
            if (!strcmp(v, "sequential")) o->mode = LAB_SEQUENTIAL;
            else if (!strcmp(v, "static")) o->mode = LAB_STATIC;
            else if (!strcmp(v, "atomic")) o->mode = LAB_ATOMIC;
            else if (!strcmp(v, "mutex")) o->mode = LAB_MUTEX;
            else return -1;
            explicit_mode = true;
        } else return -1;
    }
    if ((!strcmp(argv[1], "compare") || !strcmp(argv[1], "benchmark")) &&
        explicit_mode) return -1;
    if (strcmp(argv[1], "benchmark") && explicit_trials) return -1;
    return 0;
}

static int check(const lab_config *c, lab_mode mode,
                 uint64_t *reference, uint64_t expected) {
    uint64_t *actual = calloc(c->jobs, sizeof(*actual));
    uint64_t digest = 0;
    if (!actual) return -1;
    int ok = lab_run(c, mode, actual, &digest) == 0 &&
             digest == expected &&
             memcmp(reference, actual, c->jobs * sizeof(*actual)) == 0;
    free(actual);
    return ok ? 0 : -1;
}

static int compare(const lab_config *c) {
    uint64_t *reference = calloc(c->jobs, sizeof(*reference));
    uint64_t digest = 0;
    if (!reference) return 2;
    if (lab_run(c, LAB_SEQUENTIAL, reference, &digest) != 0) {
        free(reference); return 2;
    }
    for (int mode = LAB_STATIC; mode <= LAB_MUTEX; ++mode) {
        if (check(c, (lab_mode)mode, reference, digest) != 0) {
            fprintf(stderr, "MISMATCH: %s\n", lab_mode_name((lab_mode)mode));
            free(reference); return 2;
        }
    }
    printf("MATCH jobs=%zu work=%u threads=%u profile=%s seed=%" PRIu64
           " digest=%016" PRIx64 "\n", c->jobs, c->work, c->threads,
           lab_profile_name(c->profile), c->seed, digest);
    free(reference);
    return 0;
}

static int increasing_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static double elapsed(struct timespec a, struct timespec b) {
    return (double)(b.tv_sec-a.tv_sec) +
           (double)(b.tv_nsec-a.tv_nsec) / 1000000000.0;
}
static int benchmark(const options *o) {
    const lab_config *c = &o->config;
    uint64_t *reference = calloc(c->jobs, sizeof(*reference));
    uint64_t *results = calloc(c->jobs, sizeof(*results));
    double *samples = calloc(o->trials, sizeof(*samples));
    uint64_t expected = 0, digest = 0;
    if (!reference || !results || !samples) {
        free(reference); free(results); free(samples); return 2;
    }
    if (lab_run(c, LAB_SEQUENTIAL, reference, &expected) != 0) goto failure;
    printf("# synthetic workload; jobs=%zu work=%u threads=%u seed=%" PRIu64
           " profile=%s; median of %u trials; includes thread start/join\n",
           c->jobs, c->work, c->threads, c->seed,
           lab_profile_name(c->profile), o->trials);
    puts("mode,trials,median_ms,jobs_per_second,digest");
    for (int mode = LAB_SEQUENTIAL; mode <= LAB_MUTEX; ++mode) {
        /* Warm-up is outside measurement; each timed trial is a fresh run. */
        if (lab_run(c, (lab_mode)mode, results, &digest) != 0 ||
            digest != expected ||
            memcmp(reference, results, c->jobs * sizeof(*results))) goto mismatch;
        for (unsigned trial = 0; trial < o->trials; ++trial) {
            struct timespec a,b;
            if (clock_gettime(CLOCK_MONOTONIC,&a) != 0) goto failure;
            if (lab_run(c, (lab_mode)mode, results, &digest) != 0) goto failure;
            if (clock_gettime(CLOCK_MONOTONIC,&b) != 0) goto failure;
            samples[trial] = elapsed(a,b);
            /* Full correctness comparison remains outside the timing. */
            if (digest != expected ||
                memcmp(reference, results, c->jobs * sizeof(*results))) goto mismatch;
        }
        qsort(samples, o->trials, sizeof(*samples), increasing_double);
        double median = (o->trials & 1u) ? samples[o->trials/2] :
            (samples[o->trials/2-1] + samples[o->trials/2]) / 2.0;
        printf("%s,%u,%.6f,%.3f,%016" PRIx64 "\n",
               lab_mode_name((lab_mode)mode), o->trials, median * 1000.0,
               median > 0.0 ? c->jobs / median : 0.0, expected);
    }
    free(reference); free(results); free(samples);
    return 0;
mismatch:
    fputs("ERROR: a scheduling mode disagreed with sequential results\n", stderr);
failure:
    free(reference); free(results); free(samples);
    return 2;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--help")) { usage(stdout); return 0; }
    if (argc < 2 || (strcmp(argv[1], "run") &&
                     strcmp(argv[1], "compare") &&
                     strcmp(argv[1], "benchmark"))) {
        usage(stderr); return 2;
    }
    options o;
    int status = parse(argc, argv, &o);
    if (status == 1) { usage(stdout); return 0; }
    if (status != 0) { usage(stderr); return 2; }
    if (!strcmp(argv[1], "compare")) return compare(&o.config);
    if (!strcmp(argv[1], "benchmark")) return benchmark(&o);
    uint64_t *results = calloc(o.config.jobs, sizeof(*results));
    uint64_t digest = 0;
    if (!results) return 2;
    status = lab_run(&o.config, o.mode, results, &digest);
    free(results);
    if (status != 0) { fputs("synthetic workload execution failed\n",stderr); return 2; }
    printf("mode=%s jobs=%zu work=%u threads=%u profile=%s seed=%" PRIu64
           " digest=%016" PRIx64 "\n",lab_mode_name(o.mode),
           o.config.jobs,o.config.work,o.config.threads,
           lab_profile_name(o.config.profile),o.config.seed,digest);
    return 0;
}
