#include "hive.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *file, *format;
    unsigned threads, ticks, iterations;
} options;
static void usage(FILE *f) {
    fputs("Warehouse Hive (simulation only; no physical robot interface)\n"
          "Usage:\n"
          "  warehouse-sim demo [--threads 0..8] [--ticks N] [--format text|json]\n"
          "  warehouse-sim run FILE.wh [--threads 0..8] [--ticks N] [--format text|json]\n"
          "  warehouse-sim compare FILE.wh [--threads 1..8] [--ticks N]\n"
          "  warehouse-sim benchmark FILE.wh [--threads 1..8] [--ticks N] [--iterations N]\n"
          "  warehouse-sim --help\n"
          "Threads=0: sequential; other values use persistent parallel planning workers.\n"
          "Exit 0: all complete / comparison agrees; 1: incomplete/stalled; 2: error.\n",f);
}
static int parse_uint(const char *s, unsigned max, bool allow_zero, unsigned *out) {
    char *end = NULL;
    unsigned long n;
    if (!s || !*s || s[0] == '-') return -1;
    errno = 0;
    n = strtoul(s,&end,10);
    if (errno || !end || *end || n > max || (!allow_zero && !n)) return -1;
    *out = (unsigned)n;
    return 0;
}
static int parse(int argc, char **argv, options *o) {
    *o = (options){.file=NULL,.format="text",.threads=4,.ticks=200,.iterations=10};
    for (int i = 2; i < argc; ++i) {
        const char *a=argv[i];
        if (!strcmp(a,"--threads") || !strcmp(a,"--ticks") ||
            !strcmp(a,"--iterations") || !strcmp(a,"--format")) {
            if (++i == argc) return -1;
            if (!strcmp(a,"--threads")) {
                if (parse_uint(argv[i],8,true,&o->threads)) return -1;
            } else if (!strcmp(a,"--ticks")) {
                if (parse_uint(argv[i],100000,false,&o->ticks)) return -1;
            } else if (!strcmp(a,"--iterations")) {
                if (parse_uint(argv[i],10000,false,&o->iterations)) return -1;
            } else if (strcmp(argv[i],"text") && strcmp(argv[i],"json")) return -1;
            else o->format=argv[i];
        } else if (*a == '-' || o->file) return -1;
        else o->file=a;
    }
    return 0;
}
static int load(const char *path, wh_world *world) {
    FILE *in=fopen(path,"r");
    if (!in) { fprintf(stderr,"%s: %s\n",path,strerror(errno)); return -1; }
    char error[160] = {0};
    int rc=wh_load(in,world,error,sizeof(error));
    if (fclose(in)) rc=-1;
    if (rc) fprintf(stderr,"%s: %s\n",path,*error ? error : "file read error");
    return rc;
}
static void show(const wh_world *w, const char *format) {
    if (!strcmp(format,"json")) {
        printf("{\"ticks\":%u,\"orders\":%d,\"completed\":%u,\"moves\":%u,"
               "\"waits\":%u,\"conflicts\":%u,\"stalled\":%s,\"complete\":%s,"
               "\"robots\":[",w->ticks,w->norders,w->completed,w->moves,
               w->waits,w->conflicts,w->stalled?"true":"false",
               w->completed==(unsigned)w->norders?"true":"false");
        for (int i=0;i<w->nrobots;++i)
            printf("%s{\"id\":%d,\"x\":%d,\"y\":%d,\"order\":%d}",
                   i?",":"",i+1,w->robots[i].x,w->robots[i].y,w->robots[i].order);
        puts("]}");
    } else wh_print(stdout,w);
}
static double elapsed(struct timespec a,struct timespec b) {
    return (double)(b.tv_sec-a.tv_sec)+(double)(b.tv_nsec-a.tv_nsec)/1e9;
}
static int compare(const wh_world *seed, const options *o) {
    wh_world a=*seed,b=*seed;
    int ra=wh_run(&a,0,o->ticks), rb=wh_run(&b,o->threads,o->ticks);
    if (ra<0 || rb<0 || ra!=rb || !wh_equal(&a,&b)) {
        fputs("FAIL: sequential and parallel world states disagree\n",stderr);
        return 2;
    }
    printf("MATCH: status=%s ticks=%u completed=%u/%d moves=%u waits=%u conflicts=%u\n",
           ra==0?"complete":"unfinished",a.ticks,a.completed,a.norders,
           a.moves,a.waits,a.conflicts);
    return 0;
}
static int benchmark(const wh_world *seed,const options *o) {
    if (!o->threads) { fputs("benchmark requires --threads 1..8\n",stderr); return 2; }
    puts("mode,iterations,elapsed_ms,mean_ms_per_trial,completed_orders_per_second");
    for (int mode=0;mode<2;++mode) {
        struct timespec a,b;
        wh_world previous = *seed;
        int prior_result=-2;
        if (clock_gettime(CLOCK_MONOTONIC,&a)) return 2;
        for (unsigned i=0;i<o->iterations;++i) {
            wh_world run = *seed;
            int rc=wh_run(&run,mode?o->threads:0,o->ticks);
            if (rc<0) { fputs("simulation error\n",stderr); return 2; }
            if (mode==0) { previous=run; prior_result=rc; }
            else if (prior_result!=rc || !wh_equal(&previous,&run)) {
                fputs("benchmark correctness mismatch\n",stderr); return 2;
            }
        }
        if (clock_gettime(CLOCK_MONOTONIC,&b)) return 2;
        double seconds=elapsed(a,b);
        printf("%s,%u,%.6f,%.6f,%.3f\n",
               mode?"parallel":"sequential",o->iterations,seconds*1000.0,
               seconds*1000.0/o->iterations,
               seconds>0 ? (double)previous.completed*o->iterations/seconds : 0.0);
    }
    return 0;
}
int main(int argc,char **argv) {
    options o;
    if (argc<2 || (argc==2 && !strcmp(argv[1],"--help"))) {
        usage(argc<2?stderr:stdout); return argc<2?2:0;
    }
    if (parse(argc,argv,&o)) { usage(stderr); return 2; }
    wh_world world;
    if (!strcmp(argv[1],"demo")) {
        if (o.file) { usage(stderr); return 2; }
        wh_demo(&world);
    } else if (!strcmp(argv[1],"run") || !strcmp(argv[1],"compare") ||
               !strcmp(argv[1],"benchmark")) {
        if (!o.file || load(o.file,&world)) { usage(stderr); return 2; }
    } else { usage(stderr); return 2; }
    if (!strcmp(argv[1],"compare")) {
        if (!o.threads || strcmp(o.format,"text")) return 2;
        return compare(&world,&o);
    }
    if (!strcmp(argv[1],"benchmark")) {
        if (strcmp(o.format,"text")) return 2;
        return benchmark(&world,&o);
    }
    int rc=wh_run(&world,o.threads,o.ticks);
    if (rc<0) { fputs("simulation safety or worker failure\n",stderr); return 2; }
    show(&world,o.format);
    return rc;
}
