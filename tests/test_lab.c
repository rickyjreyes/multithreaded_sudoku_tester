#include "lab.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void agreement(lab_config c) {
    uint64_t *baseline = calloc(c.jobs, sizeof(*baseline));
    uint64_t *actual = calloc(c.jobs, sizeof(*actual));
    uint64_t d = 0, other = 0;
    assert(baseline && actual);
    assert(lab_run(&c, LAB_SEQUENTIAL, baseline, &d) == 0);
    for (unsigned workers = 1; workers <= 32; workers *= 2) {
        c.threads = workers;
        for (int m = LAB_STATIC; m <= LAB_MUTEX; ++m) {
            for (int repeat = 0; repeat < 3; ++repeat) {
                assert(lab_run(&c, (lab_mode)m, actual, &other) == 0);
                assert(d == other);
                assert(memcmp(baseline, actual, c.jobs * sizeof(*actual)) == 0);
            }
        }
    }
    free(baseline); free(actual);
}
static void configurations(void) {
    static const size_t jobs[] = {1,2,7,8,31,32,65,257};
    for (int profile = LAB_UNIFORM; profile <= LAB_SKEWED; ++profile)
        for (size_t i = 0; i < sizeof(jobs)/sizeof(jobs[0]); ++i) {
            lab_config c = {
                .jobs=jobs[i], .work=2u + (unsigned)i,
                .threads=4, .seed=UINT64_C(0xfedcba9876543210),
                .profile=(lab_profile)profile
            };
            agreement(c);
        }
}
static void invalid(void) {
    lab_config c = {.jobs=4,.work=5,.threads=2,.seed=1,.profile=LAB_UNIFORM};
    uint64_t results[4] = {0}, digest = 0;
    assert(lab_run(NULL, LAB_SEQUENTIAL, results, &digest) != 0);
    assert(lab_run(&c, LAB_SEQUENTIAL, NULL, &digest) != 0);
    assert(lab_run(&c, LAB_SEQUENTIAL, results, NULL) != 0);
    assert(lab_run(&c, (lab_mode)99, results, &digest) != 0);
    c.jobs=0; assert(lab_run(&c, LAB_STATIC, results, &digest) != 0);
    c.jobs=LAB_MAX_JOBS+1u; assert(lab_run(&c, LAB_STATIC, results, &digest) != 0);
    c.jobs=4; c.threads=0; assert(lab_run(&c, LAB_ATOMIC, results, &digest) != 0);
    c.threads=LAB_MAX_THREADS+1u; assert(lab_run(&c, LAB_MUTEX, results, &digest) != 0);
    c.threads=2; c.work=0; assert(lab_run(&c, LAB_SEQUENTIAL, results, &digest) != 0);
    c.work=LAB_MAX_WORK+1u; assert(lab_run(&c, LAB_SEQUENTIAL, results, &digest) != 0);
    c.work=5; c.profile=(lab_profile)99;
    assert(lab_run(&c, LAB_SEQUENTIAL, results, &digest) != 0);
}
static void seed_changes_results(void) {
    lab_config c={.jobs=8,.work=10,.threads=2,.seed=1,.profile=LAB_UNIFORM};
    uint64_t a[8],b[8],d1,d2;
    assert(lab_run(&c,LAB_SEQUENTIAL,a,&d1)==0);
    ++c.seed;
    assert(lab_run(&c,LAB_SEQUENTIAL,b,&d2)==0);
    assert(memcmp(a,b,sizeof(a)) != 0 && d1 != d2);
}
int main(void) {
    configurations();
    invalid();
    seed_changes_results();
    puts("All public concurrency lab tests passed.");
    return 0;
}
