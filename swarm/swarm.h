#ifndef PUBLIC_SWARM_LAB_H
#define PUBLIC_SWARM_LAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWARM_CAPACITY 256u
#define SWARM_MAX_THREADS 32u

/* Stylized, synthetic models: NOT empirical biological/traffic predictions. */
typedef enum {
    SWARM_BEES,       /* binary local recruitment and undecided agents */
    SWARM_TRAFFIC,    /* periodic one-lane cellular automaton, rule 184 */
    SWARM_PHASE,      /* all-to-all Kuramoto-like phase coupling */
    SWARM_LIFE        /* toroidal Conway's Game of Life */
} swarm_model;

typedef enum {
    SWARM_SEQUENTIAL,
    SWARM_STATIC,
    SWARM_ATOMIC,
    SWARM_MUTEX
} swarm_schedule;

typedef struct {
    swarm_model model;
    size_t count;
    unsigned side;             /* nonzero only for square cellular grid */
    unsigned tick;
    uint64_t seed;
    double coupling;           /* used only for phase model */
    double state[SWARM_CAPACITY]; /* 0/1/2 or phase in [0, 2*pi) */
    double metric;             /* see swarm_metric_name */
    double secondary;
} swarm_world;

const char *swarm_model_name(swarm_model model);
const char *swarm_schedule_name(swarm_schedule schedule);
const char *swarm_metric_name(swarm_model model);
const char *swarm_secondary_name(swarm_model model);

/* count in [4,256]; Life requires a perfect square.
 * coupling finite in [0,8]. Each model has a documented fixed toy rule. */
int swarm_init(swarm_world *world, swarm_model model, size_t count,
               uint64_t seed, double coupling);

/* Compute every next-state element from the same immutable previous state;
 * all parallel modes publish the full new state only after workers join.
 * On thread creation failure the world state remains unchanged. */
int swarm_step(swarm_world *world, swarm_schedule schedule, unsigned threads);

/* Bit-exact state comparison for identical seed/model/parameters and binary. */
bool swarm_equal(const swarm_world *a, const swarm_world *b);
uint64_t swarm_digest(const swarm_world *world);

#endif
