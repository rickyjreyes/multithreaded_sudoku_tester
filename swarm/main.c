#include "swarm.h"
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    swarm_model model;
    swarm_schedule schedule;
    size_t agents;
    unsigned steps, threads, trials;
    uint64_t seed;
    double coupling;
} options;
static void usage(FILE *f) {
    fputs(
        "Public Swarm Lab — synthetic analogies, not empirical domain simulators\n"
        "Usage: swarm-lab run|compare|benchmark [options]\n"
        "  --model bees|traffic|phase|life  (default bees)\n"
        "  --agents N        4..256; perfect square for life (default 64)\n"
        "  --steps N         1..10000 (default 30)\n"
        "  --threads N       1..32 (default 4)\n"
        "  --seed N          unsigned 64-bit integer (default 42)\n"
        "  --coupling X      phase coupling, finite 0..8 (default 1.5)\n"
        "  --scheduler sequential|static|atomic|mutex (run only)\n"
        "  --trials N        1..100 (benchmark only; default 7)\n"
        "  --help\n"
        "Compare checks every complete state after every step, across all modes.\n"
        "Benchmark measures each full run, including thread start/join per step;\n"
        "correctness checks are outside the timing interval.\n", f);
}
static int integer(const char *s,uint64_t min,uint64_t max,uint64_t *out) {
    char *end=NULL;
    unsigned long long value;
    if (!s || !*s || *s == '-' || *s == '+') return -1;
    errno=0; value=strtoull(s,&end,10);
    if (errno || !end || *end || (uint64_t)value < min ||
        (uint64_t)value > max) return -1;
    *out=(uint64_t)value; return 0;
}
static int parse(int argc,char **argv,options *o) {
    *o=(options){.model=SWARM_BEES,.schedule=SWARM_SEQUENTIAL,.agents=64,
        .steps=30,.threads=4,.trials=7,.seed=42,.coupling=1.5};
    bool special_schedule=false,special_trials=false;
    for (int i=2;i<argc;++i) {
        const char *key=argv[i];
        uint64_t num;
        if (!strcmp(key,"--help")) return 1;
        if (++i==argc) return -1;
        const char *val=argv[i];
        if (!strcmp(key,"--model")) {
            if (!strcmp(val,"bees")) o->model=SWARM_BEES;
            else if (!strcmp(val,"traffic")) o->model=SWARM_TRAFFIC;
            else if (!strcmp(val,"phase")) o->model=SWARM_PHASE;
            else if (!strcmp(val,"life")) o->model=SWARM_LIFE;
            else return -1;
        } else if (!strcmp(key,"--scheduler")) {
            if (!strcmp(val,"sequential")) o->schedule=SWARM_SEQUENTIAL;
            else if (!strcmp(val,"static")) o->schedule=SWARM_STATIC;
            else if (!strcmp(val,"atomic")) o->schedule=SWARM_ATOMIC;
            else if (!strcmp(val,"mutex")) o->schedule=SWARM_MUTEX;
            else return -1;
            special_schedule=true;
        } else if (!strcmp(key,"--agents")) {
            if (integer(val,4,SWARM_CAPACITY,&num)) return -1;
            o->agents=(size_t)num;
        } else if (!strcmp(key,"--steps")) {
            if (integer(val,1,10000,&num)) return -1;
            o->steps=(unsigned)num;
        } else if (!strcmp(key,"--threads")) {
            if (integer(val,1,SWARM_MAX_THREADS,&num)) return -1;
            o->threads=(unsigned)num;
        } else if (!strcmp(key,"--trials")) {
            if (integer(val,1,100,&num)) return -1;
            o->trials=(unsigned)num;
            special_trials=true;
        } else if (!strcmp(key,"--seed")) {
            if (integer(val,0,UINT64_MAX,&num)) return -1;
            o->seed=num;
        } else if (!strcmp(key,"--coupling")) {
            char *end=NULL;
            errno=0;
            double x=strtod(val,&end);
            if (!*val || errno || !end || *end || !isfinite(x) ||
                x<0.0 || x>8.0) return -1;
            o->coupling=x;
        } else return -1;
    }
    if (special_schedule && strcmp(argv[1],"run")) return -1;
    if (special_trials && strcmp(argv[1],"benchmark")) return -1;
    return 0;
}
static int initialize(const options *o,swarm_world *w) {
    if (swarm_init(w,o->model,o->agents,o->seed,o->coupling)==0) return 0;
    fputs("Invalid configuration; life requires a square agent count (e.g. 64)\n",stderr);
    return -1;
}
static void print_state(const swarm_world *w,swarm_schedule s,unsigned threads) {
    printf("model=%s scheduler=%s agents=%zu steps=%u threads=%u "
           "%s=%.9f %s=%.9f digest=%016" PRIx64 "\n",
           swarm_model_name(w->model),swarm_schedule_name(s),w->count,w->tick,
           threads,swarm_metric_name(w->model),w->metric,
           swarm_secondary_name(w->model),w->secondary,swarm_digest(w));
}
static int run(const options *o) {
    swarm_world w;
    if (initialize(o,&w)) return 2;
    for (unsigned step=0;step<o->steps;++step)
        if (swarm_step(&w,o->schedule,o->threads)) {
            fputs("swarm execution error\n",stderr);return 2;
        }
    print_state(&w,o->schedule,o->threads);
    return 0;
}
static int compare(const options *o) {
    swarm_world states[4];
    for (int i=0;i<4;++i)
        if (initialize(o,&states[i])) return 2;
    for (unsigned step=0;step<o->steps;++step) {
        for (int s=0;s<4;++s)
            if (swarm_step(&states[s],(swarm_schedule)s,o->threads)) {
                fputs("swarm execution error\n",stderr);return 2;
            }
        for (int s=1;s<4;++s) if (!swarm_equal(&states[0],&states[s])) {
            fprintf(stderr,"MISMATCH model=%s at step=%u scheduler=%s\n",
                    swarm_model_name(o->model),step+1,
                    swarm_schedule_name((swarm_schedule)s));
            return 2;
        }
    }
    printf("MATCH model=%s agents=%zu steps=%u threads=%u seed=%" PRIu64
           " %s=%.9f digest=%016" PRIx64 "\n",
           swarm_model_name(o->model),o->agents,o->steps,o->threads,o->seed,
           swarm_metric_name(o->model),states[0].metric,swarm_digest(&states[0]));
    return 0;
}
static double seconds(struct timespec a,struct timespec b) {
    return (double)(b.tv_sec-a.tv_sec) +
           (double)(b.tv_nsec-a.tv_nsec)/1000000000.0;
}
static int numeric_order(const void *a,const void *b) {
    double x=*(const double *)a,y=*(const double *)b;
    return (x>y)-(x<y);
}
static int benchmark(const options *o) {
    swarm_world expected,actual;
    double samples[100];
    if (initialize(o,&expected)) return 2;
    for (unsigned step=0;step<o->steps;++step)
        if (swarm_step(&expected,SWARM_SEQUENTIAL,o->threads)) return 2;
    printf("# synthetic toy model; seed=%" PRIu64
           "; per-step thread startup included; median of %u trials\n",
           o->seed,o->trials);
    puts("model,scheduler,agents,steps,threads,trials,median_ms,steps_per_second,metric,digest");
    for (int s=0;s<4;++s) {
        /* Warm-up and full verification are not in the timed region. */
        if (initialize(o,&actual)) return 2;
        for (unsigned step=0;step<o->steps;++step)
            if (swarm_step(&actual,(swarm_schedule)s,o->threads)) return 2;
        if (!swarm_equal(&actual,&expected)) {
            fprintf(stderr,"MISMATCH warm-up scheduler=%s\n",
                    swarm_schedule_name((swarm_schedule)s));return 2;
        }
        for (unsigned trial=0;trial<o->trials;++trial) {
            struct timespec a,b;
            if (initialize(o,&actual) ||
                clock_gettime(CLOCK_MONOTONIC,&a)) return 2;
            for (unsigned step=0;step<o->steps;++step)
                if (swarm_step(&actual,(swarm_schedule)s,o->threads)) return 2;
            if (clock_gettime(CLOCK_MONOTONIC,&b)) return 2;
            samples[trial]=seconds(a,b);
            if (!swarm_equal(&actual,&expected)) {
                fprintf(stderr,"MISMATCH scheduler=%s trial=%u\n",
                        swarm_schedule_name((swarm_schedule)s),trial+1);return 2;
            }
        }
        qsort(samples,o->trials,sizeof(samples[0]),numeric_order);
        double median=(o->trials&1u)?samples[o->trials/2]:
            (samples[o->trials/2-1]+samples[o->trials/2])/2.0;
        printf("%s,%s,%zu,%u,%u,%u,%.6f,%.3f,%.9f,%016" PRIx64 "\n",
               swarm_model_name(o->model),swarm_schedule_name((swarm_schedule)s),
               o->agents,o->steps,o->threads,o->trials,1000.0*median,
               median>0.0?o->steps/median:0.0,expected.metric,
               swarm_digest(&expected));
    }
    return 0;
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--help")) {usage(stdout);return 0;}
    if (argc<2 || (strcmp(argv[1],"run") && strcmp(argv[1],"compare") &&
                   strcmp(argv[1],"benchmark"))) {usage(stderr);return 2;}
    options o;
    int result=parse(argc,argv,&o);
    if (result==1) {usage(stdout);return 0;}
    if (result) {usage(stderr);return 2;}
    if (!strcmp(argv[1],"run")) return run(&o);
    if (!strcmp(argv[1],"compare")) return compare(&o);
    return benchmark(&o);
}
