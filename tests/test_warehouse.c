#include "hive.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int from_text(const char *s,wh_world *w) {
    FILE *f=tmpfile();
    char e[160];
    assert(f);
    assert(fputs(s,f)>=0);
    rewind(f);
    int rc=wh_load(f,w,e,sizeof(e));
    assert(fclose(f)==0);
    return rc;
}
static void same_for_threads(const wh_world *seed,unsigned ticks) {
    wh_world a=*seed;
    int ra=wh_run(&a,0,ticks);
    assert(ra>=0 && wh_safe(&a));
    for (unsigned n=1;n<=8;n*=2) {
        wh_world b=*seed;
        int rb=wh_run(&b,n,ticks);
        assert(rb==ra && wh_safe(&b));
        assert(wh_equal(&a,&b));
    }
}
static void demo_test(void) {
    wh_world w;
    wh_demo(&w);
    assert(wh_safe(&w));
    assert(w.nrobots==4 && w.norders==8);
    same_for_threads(&w,250);
}
static void conflict_test(void) {
    static const char *s="SIZE 7 5\nMAP\n#######\n#1.2..#\n#.....#\n#.....#\n#######\nORDERS\n2 1 1 2\n2 1 4 2\n";
    wh_world initial, a;
    assert(from_text(s,&initial)==0);
    a=initial;
    assert(wh_run(&a,0,1)==1);
    assert(wh_safe(&a));
    assert(a.conflicts==1 && a.moves==1);
    assert(a.robots[0].x==2 && a.robots[1].x==3);
    same_for_threads(&initial,1);
    same_for_threads(&initial,100);
}
static void blocked_test(void) {
    static const char *s="SIZE 7 5\nMAP\n#######\n#12...#\n#.....#\n#.....#\n#######\nORDERS\n2 1 3 2\n1 1 4 2\n";
    wh_world w;
    assert(from_text(s,&w)==0);
    same_for_threads(&w,30);
    assert(wh_run(&w,0,30)==1);
    assert(w.stalled && w.ticks==16 && w.completed==0 && wh_safe(&w));
}
static void invalid_test(void) {
    wh_world w;
    assert(from_text("SIZE 2 2\nMAP\n12\n..\nORDERS\n1 0 1 1\n",&w)!=0);
    assert(from_text("SIZE 5 3\nMAP\n#####\n#1..#\n#####\nORDERS\n0 0 2 1\n",&w)!=0);
    assert(from_text("SIZE 5 3\nMAP\n#####\n#1.1#\n#####\nORDERS\n2 1 3 1\n",&w)!=0);
    assert(from_text("SIZE 5 3\nMAP\n#####\n#2..#\n#####\nORDERS\n2 1 3 1\n",&w)!=0);
}
static void finished_test(void) {
    wh_world w;
    assert(from_text("SIZE 5 4\nMAP\n#####\n#1..#\n#...#\n#####\nORDERS\n2 1 3 1\n",&w)==0);
    same_for_threads(&w,20);
    assert(wh_run(&w,0,20)==0);
    assert(w.completed==1 && w.orders[0].state==WH_DONE && w.ticks>=2);
    assert(wh_run(&w,4,20)==0); /* already complete: no new movement */
    assert(w.completed==1 && wh_safe(&w));
}
int main(void) {
    demo_test();
    conflict_test();
    blocked_test();
    invalid_test();
    finished_test();
    puts("All warehouse coordination tests passed.");
    return 0;
}
