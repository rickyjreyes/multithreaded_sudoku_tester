#ifndef PUBLIC_CONCURRENCY_LAB_H
#define PUBLIC_CONCURRENCY_LAB_H

#include <stddef.h>
#include <stdint.h>

/* Deliberately generic synthetic CPU workload; no operational models or input. */
#define LAB_MAX_JOBS 65536u
#define LAB_MAX_THREADS 32u
#define LAB_MAX_WORK 100000u

typedef enum {
    LAB_SEQUENTIAL,
    LAB_STATIC,
    LAB_ATOMIC,
    LAB_MUTEX
} lab_mode;

typedef enum {
    LAB_UNIFORM,
    LAB_SKEWED
} lab_profile;

typedef struct {
    size_t jobs;
    unsigned work;
    unsigned threads;
    uint64_t seed;
    lab_profile profile;
} lab_config;

/* Writes exactly config->jobs independent, index-addressed results.
 * All execution modes perform the same deterministic calculations.
 * digest is a non-cryptographic summary reduced in index order after join.
 * Returns 0 on success and -1 on invalid configuration/resource failure. */
int lab_run(const lab_config *config, lab_mode mode,
            uint64_t *results, uint64_t *digest);

const char *lab_mode_name(lab_mode mode);
const char *lab_profile_name(lab_profile profile);

#endif
