#ifndef WAREHOUSE_HIVE_H
#define WAREHOUSE_HIVE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define WH_WIDTH 32
#define WH_HEIGHT 32
#define WH_ROBOTS 8
#define WH_ORDERS 128

typedef enum { WH_PENDING, WH_PICKUP, WH_DELIVERY, WH_DONE } wh_state;
typedef struct { int x, y, order; } wh_robot;
typedef struct {
    int px, py, dx, dy, robot;
    wh_state state;
} wh_order;
typedef struct {
    int width, height, nrobots, norders;
    char floor[WH_HEIGHT][WH_WIDTH];
    wh_robot robots[WH_ROBOTS];
    wh_order orders[WH_ORDERS];
    unsigned ticks, moves, waits, conflicts, completed, stall_ticks;
    bool stalled;
} wh_world;

/* . floor; # wall; A aisle; H home; P pickup. 1..8 mark initial robots.
 * Positions are (x,y), origin at top left. All coordinates are zero-based.
 * Orders are four integers: pickup_x pickup_y delivery_x delivery_y. */
int wh_load(FILE *in, wh_world *world, char *error, size_t error_size);
void wh_demo(wh_world *world);
bool wh_safe(const wh_world *world);
/* threads=0 runs sequential proposals; 1..8 runs a persistent planning pool.
 * Result 0: all orders done; 1: tick limit or stall limit reached; -1: error.
 * Both modes use identical snapshot planning and serial deterministic commits. */
int wh_run(wh_world *world, unsigned threads, unsigned max_ticks);
bool wh_equal(const wh_world *a, const wh_world *b);
void wh_print(FILE *out, const wh_world *world);
#endif
