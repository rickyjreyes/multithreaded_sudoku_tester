#include "swarm.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static size_t population(const swarm_world *w) {
    size_t count=0;
    for (size_t i=0;i<w->count;++i) count += w->state[i]==1.0;
    return count;
}
static void agreement(swarm_model model,size_t n,unsigned steps,uint64_t seed,
                      double coupling) {
    swarm_world base,other;
    assert(swarm_init(&base,model,n,seed,coupling)==0);
    for (unsigned workers=1;workers<=32;workers*=2) {
        for (int mode=SWARM_STATIC;mode<=SWARM_MUTEX;++mode) {
            other=base;
            swarm_world sequential=base;
            for (unsigned step=0;step<steps;++step) {
                assert(swarm_step(&sequential,SWARM_SEQUENTIAL,workers)==0);
                assert(swarm_step(&other,(swarm_schedule)mode,workers)==0);
                assert(swarm_equal(&sequential,&other));
                assert(swarm_digest(&sequential)==swarm_digest(&other));
                assert(isfinite(other.metric) && other.metric>=0.0 &&
                       other.metric<=1.00000001);
                assert(isfinite(other.secondary));
            }
        }
    }
}
static void all_models(void) {
    for (int model=SWARM_BEES;model<=SWARM_LIFE;++model) {
        agreement((swarm_model)model,64,5,42,1.5);
        agreement((swarm_model)model,16,7,123,0.4);
        if (model != SWARM_LIFE)
            agreement((swarm_model)model,17,3,999,2.0);
    }
}
static void traffic_conservation(void) {
    swarm_world w;
    assert(swarm_init(&w,SWARM_TRAFFIC,127,7,1.5)==0);
    size_t initial=population(&w);
    assert(initial>0 && initial<127);
    for (int t=0;t<100;++t) {
        assert(swarm_step(&w,SWARM_ATOMIC,8)==0);
        assert(population(&w)==initial);
    }
}
static void bee_commitment(void) {
    swarm_world w;
    assert(swarm_init(&w,SWARM_BEES,128,17,1.5)==0);
    double last=w.metric;
    for (int t=0;t<40;++t) {
        assert(swarm_step(&w,SWARM_STATIC,4)==0);
        assert(w.metric>=last);
        last=w.metric;
        for (size_t i=0;i<w.count;++i)
            assert(w.state[i]==0.0 || w.state[i]==1.0 || w.state[i]==2.0);
    }
}
static void phase_bounds(void) {
    swarm_world w;
    assert(swarm_init(&w,SWARM_PHASE,32,7,1.5)==0);
    for (int t=0;t<30;++t) {
        assert(swarm_step(&w,SWARM_MUTEX,4)==0);
        for (size_t i=0;i<w.count;++i)
            assert(isfinite(w.state[i]) && w.state[i]>=0.0 &&
                   w.state[i]<6.283185307179586476925286766559);
    }
}
static void life_blinker(void) {
    swarm_world w={0};
    assert(swarm_init(&w,SWARM_LIFE,25,10,1.5)==0);
    memset(w.state,0,sizeof(w.state));
    w.state[2*5+1]=1.0; w.state[2*5+2]=1.0; w.state[2*5+3]=1.0;
    assert(swarm_step(&w,SWARM_SEQUENTIAL,1)==0);
    assert(w.state[1*5+2]==1.0 && w.state[2*5+2]==1.0 &&
           w.state[3*5+2]==1.0);
    assert(population(&w)==3);
    assert(swarm_step(&w,SWARM_ATOMIC,3)==0);
    assert(w.state[2*5+1]==1.0 && w.state[2*5+2]==1.0 &&
           w.state[2*5+3]==1.0 && population(&w)==3);
}
static void invalid(void) {
    swarm_world w;
    assert(swarm_init(NULL,SWARM_BEES,64,1,1.5)!=0);
    assert(swarm_init(&w,SWARM_LIFE,30,1,1.5)!=0);
    assert(swarm_init(&w,SWARM_PHASE,3,1,1.5)!=0);
    assert(swarm_init(&w,SWARM_TRAFFIC,257,1,1.5)!=0);
    assert(swarm_init(&w,SWARM_BEES,64,1,-1.0)!=0);
    assert(swarm_init(&w,SWARM_BEES,64,1,NAN)!=0);
    assert(swarm_init(&w,SWARM_BEES,64,1,INFINITY)!=0);
    assert(swarm_init(&w,SWARM_BEES,64,1,1.5)==0);
    assert(swarm_step(&w,SWARM_STATIC,0)!=0);
    assert(swarm_step(&w,SWARM_ATOMIC,33)!=0);
    assert(swarm_step(&w,(swarm_schedule)77,1)!=0);
    assert(swarm_step(NULL,SWARM_SEQUENTIAL,1)!=0);
}
int main(void) {
    all_models();
    traffic_conservation();
    bee_commitment();
    phase_bounds();
    life_blinker();
    invalid();
    puts("All cross-domain swarm tests passed.");
    return 0;
}
