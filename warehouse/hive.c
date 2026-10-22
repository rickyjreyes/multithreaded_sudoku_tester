#include "hive.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int x, y; } position;
typedef struct {
    pthread_t ids[WH_ROBOTS];
    unsigned workers, started, generation, remaining;
    int next;
    bool stop;
    pthread_mutex_t mutex;
    pthread_cond_t go, done;
    const wh_world *world;
    position proposals[WH_ROBOTS];
} planning_pool;

static bool within(const wh_world *w, int x, int y) {
    return x >= 0 && y >= 0 && x < w->width && y < w->height;
}
static bool walkable(const wh_world *w, int x, int y) {
    return within(w, x, y) && w->floor[y][x] != '#';
}
static void error_at(char *e, size_t n, const char *message) {
    if (e && n) snprintf(e, n, "%s", message);
}
static void trim(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\r' || s[n-1] == '\n' ||
                 s[n-1] == ' ' || s[n-1] == '\t')) s[--n] = '\0';
}
static bool header_line(FILE *in, char *line, size_t len) {
    while (fgets(line, (int)len, in)) {
        trim(line);
        if (*line && *line != '#') return true;
    }
    return false;
}
int wh_load(FILE *in, wh_world *w, char *e, size_t n) {
    char line[256], extra;
    bool found[WH_ROBOTS] = {false};
    if (!in || !w || !e || !n) return -1;
    memset(w, 0, sizeof(*w));
    if (!header_line(in, line, sizeof(line)) ||
        sscanf(line, "SIZE %d %d %c", &w->width, &w->height, &extra) != 2 ||
        w->width < 3 || w->width > WH_WIDTH || w->height < 3 ||
        w->height > WH_HEIGHT) {
        error_at(e, n, "expected SIZE width height (3..32)"); return -1;
    }
    if (!header_line(in, line, sizeof(line)) || strcmp(line, "MAP")) {
        error_at(e, n, "expected MAP"); return -1;
    }
    for (int y = 0; y < w->height; ++y) {
        if (!fgets(line, sizeof(line), in)) {
            error_at(e, n, "missing map rows"); return -1;
        }
        trim(line);
        if (strlen(line) != (size_t)w->width) {
            error_at(e, n, "map row has incorrect width"); return -1;
        }
        for (int x = 0; x < w->width; ++x) {
            char cell = line[x];
            if (cell >= '1' && cell <= '8') {
                int id = cell - '1';
                if (found[id]) {
                    error_at(e, n, "duplicate robot ID"); return -1;
                }
                found[id] = true;
                w->robots[id] = (wh_robot){x, y, -1};
                ++w->nrobots;
                w->floor[y][x] = '.';
            } else if (cell == '#' || cell == '.' || cell == 'A' ||
                       cell == 'P' || cell == 'H') w->floor[y][x] = cell;
            else {
                error_at(e, n, "invalid map character"); return -1;
            }
        }
    }
    if (w->nrobots == 0) { error_at(e, n, "no robots"); return -1; }
    for (int i = 0; i < w->nrobots; ++i)
        if (!found[i]) { error_at(e, n, "robot IDs must start at 1 and be contiguous"); return -1; }
    if (!header_line(in, line, sizeof(line)) || strcmp(line, "ORDERS")) {
        error_at(e, n, "expected ORDERS"); return -1;
    }
    while (fgets(line, sizeof(line), in)) {
        trim(line);
        if (!*line || line[0] == '#') continue;
        int px, py, dx, dy;
        if (sscanf(line, "%d %d %d %d %c", &px, &py, &dx, &dy, &extra) != 4 ||
            !walkable(w, px, py) || !walkable(w, dx, dy)) {
            error_at(e, n, "malformed order or blocked/out-of-bounds endpoint"); return -1;
        }
        if (w->norders == WH_ORDERS) {
            error_at(e, n, "too many orders"); return -1;
        }
        w->orders[w->norders++] = (wh_order){px, py, dx, dy, -1, WH_PENDING};
    }
    if (ferror(in)) { error_at(e, n, "read error"); return -1; }
    if (!w->norders) { error_at(e, n, "no orders"); return -1; }
    e[0] = '\0';
    return 0;
}
void wh_demo(wh_world *w) {
    static const char *rows[] = {
        "############",
        "#1..A..P..2#",
        "#..........#",
        "#..##..##..#",
        "#H.A....A.P#",
        "#..##..##..#",
        "#..........#",
        "#3..P..A..4#",
        "############"
    };
    static const int orders[][4] = {
        {7,1,3,2}, {10,4,8,2}, {4,7,3,6}, {7,1,8,6},
        {4,7,1,4}, {10,4,10,7}, {7,1,3,4}, {4,7,8,4}
    };
    memset(w, 0, sizeof(*w));
    w->width = 12; w->height = 9; w->nrobots = 4; w->norders = 8;
    for (int y = 0; y < w->height; ++y)
        for (int x = 0; x < w->width; ++x) {
            char c = rows[y][x];
            if (c >= '1' && c <= '4') {
                int id = c - '1';
                w->robots[id] = (wh_robot){x, y, -1};
                w->floor[y][x] = '.';
            } else w->floor[y][x] = c;
        }
    for (int i = 0; i < w->norders; ++i)
        w->orders[i] = (wh_order){orders[i][0], orders[i][1],
                                  orders[i][2], orders[i][3], -1, WH_PENDING};
}
bool wh_safe(const wh_world *w) {
    if (!w || w->width < 3 || w->width > WH_WIDTH ||
        w->height < 3 || w->height > WH_HEIGHT ||
        w->nrobots < 1 || w->nrobots > WH_ROBOTS ||
        w->norders < 1 || w->norders > WH_ORDERS) return false;
    for (int i = 0; i < w->nrobots; ++i) {
        const wh_robot *r = &w->robots[i];
        if (!walkable(w, r->x, r->y) || r->order < -1 || r->order >= w->norders)
            return false;
        for (int j = 0; j < i; ++j)
            if (r->x == w->robots[j].x && r->y == w->robots[j].y) return false;
    }
    return true;
}
static bool occupied(const wh_world *w, int x, int y, int except) {
    for (int i = 0; i < w->nrobots; ++i)
        if (i != except && w->robots[i].x == x && w->robots[i].y == y) return true;
    return false;
}
static position proposal(const wh_world *w, int id) {
    const wh_robot *r = &w->robots[id];
    position stay = {r->x, r->y}, queue[WH_WIDTH * WH_HEIGHT];
    position first[WH_HEIGHT][WH_WIDTH];
    bool seen[WH_HEIGHT][WH_WIDTH] = {{false}};
    int head = 0, tail = 0;
    if (r->order < 0) return stay;
    const wh_order *o = &w->orders[r->order];
    int target_x = o->state == WH_PICKUP ? o->px : o->dx;
    int target_y = o->state == WH_PICKUP ? o->py : o->dy;
    if (target_x == r->x && target_y == r->y) return stay;
    queue[tail++] = stay;
    seen[r->y][r->x] = true;
    first[r->y][r->x] = stay;
    const int offsets[4][2] = {{0,-1},{1,0},{0,1},{-1,0}};
    while (head < tail) {
        position at = queue[head++];
        for (int d = 0; d < 4; ++d) {
            int nx = at.x + offsets[d][0], ny = at.y + offsets[d][1];
            if (!walkable(w,nx,ny) || seen[ny][nx] || occupied(w,nx,ny,id))
                continue;
            seen[ny][nx] = true;
            first[ny][nx] = at.x == r->x && at.y == r->y ?
                (position){nx,ny} : first[at.y][at.x];
            if (nx == target_x && ny == target_y) return first[ny][nx];
            queue[tail++] = (position){nx,ny};
        }
    }
    return stay;
}
static void *worker(void *arg) {
    planning_pool *p = arg;
    unsigned seen_generation = 0;
    pthread_mutex_lock(&p->mutex);
    for (;;) {
        while (!p->stop && seen_generation == p->generation)
            pthread_cond_wait(&p->go, &p->mutex);
        if (p->stop) break;
        seen_generation = p->generation;
        while (p->next < p->world->nrobots) {
            int id = p->next++;
            const wh_world *w = p->world;
            pthread_mutex_unlock(&p->mutex);
            p->proposals[id] = proposal(w,id);
            pthread_mutex_lock(&p->mutex);
        }
        if (--p->remaining == 0) pthread_cond_signal(&p->done);
    }
    pthread_mutex_unlock(&p->mutex);
    return NULL;
}
static int pool_open(planning_pool *p, unsigned threads) {
    memset(p,0,sizeof(*p));
    p->workers = threads;
    if (pthread_mutex_init(&p->mutex,NULL)) return -1;
    if (pthread_cond_init(&p->go,NULL)) { pthread_mutex_destroy(&p->mutex); return -1; }
    if (pthread_cond_init(&p->done,NULL)) {
        pthread_cond_destroy(&p->go); pthread_mutex_destroy(&p->mutex); return -1;
    }
    for (unsigned i = 0; i < threads; ++i) {
        if (pthread_create(&p->ids[i],NULL,worker,p)) {
            pthread_mutex_lock(&p->mutex); p->stop = true;
            pthread_cond_broadcast(&p->go); pthread_mutex_unlock(&p->mutex);
            for (unsigned j = 0; j < p->started; ++j) pthread_join(p->ids[j],NULL);
            pthread_cond_destroy(&p->done); pthread_cond_destroy(&p->go);
            pthread_mutex_destroy(&p->mutex); return -1;
        }
        ++p->started;
    }
    return 0;
}
static void pool_close(planning_pool *p) {
    pthread_mutex_lock(&p->mutex);
    p->stop = true;
    pthread_cond_broadcast(&p->go);
    pthread_mutex_unlock(&p->mutex);
    for (unsigned i = 0; i < p->started; ++i) pthread_join(p->ids[i],NULL);
    pthread_cond_destroy(&p->done); pthread_cond_destroy(&p->go);
    pthread_mutex_destroy(&p->mutex);
}
static void compute(planning_pool *p, const wh_world *w) {
    pthread_mutex_lock(&p->mutex);
    p->world = w; p->next = 0; p->remaining = p->workers;
    ++p->generation;
    pthread_cond_broadcast(&p->go);
    while (p->remaining) pthread_cond_wait(&p->done,&p->mutex);
    pthread_mutex_unlock(&p->mutex);
}
static void arrivals(wh_world *w) {
    for (int i = 0; i < w->nrobots; ++i) {
        wh_robot *r = &w->robots[i];
        if (r->order < 0) continue;
        wh_order *o = &w->orders[r->order];
        if (o->state == WH_PICKUP && r->x == o->px && r->y == o->py)
            o->state = WH_DELIVERY;
        if (o->state == WH_DELIVERY && r->x == o->dx && r->y == o->dy) {
            o->state = WH_DONE; r->order = -1; ++w->completed;
        }
    }
}
static void assign(wh_world *w) {
    for (int i = 0; i < w->nrobots; ++i) {
        if (w->robots[i].order != -1) continue;
        for (int j = 0; j < w->norders; ++j) if (w->orders[j].state == WH_PENDING) {
            w->orders[j].state = WH_PICKUP; w->orders[j].robot = i;
            w->robots[i].order = j;
            break;
        }
    }
}
static void tick(wh_world *w, planning_pool *pool) {
    unsigned before = w->completed, moved = 0;
    position proposed[WH_ROBOTS];
    bool reserved[WH_HEIGHT][WH_WIDTH] = {{false}};
    arrivals(w); assign(w); arrivals(w);
    if (pool) {
        compute(pool,w);
        for (int i = 0; i < w->nrobots; ++i) proposed[i] = pool->proposals[i];
    } else for (int i = 0; i < w->nrobots; ++i) proposed[i] = proposal(w,i);
    /* Conservative tick reservation: cells occupied at the snapshot start
     * cannot be entered this tick, even if their occupant proposes moving.
     * This prevents swaps and follow-the-leader overlap. Lower IDs win ties. */
    for (int i = 0; i < w->nrobots; ++i) {
        wh_robot *r = &w->robots[i];
        position p = proposed[i];
        if (r->order < 0) continue;
        if (p.x == r->x && p.y == r->y) { ++w->waits; continue; }
        if (!walkable(w,p.x,p.y) || occupied(w,p.x,p.y,i) ||
            reserved[p.y][p.x]) {
            ++w->conflicts; ++w->waits; continue;
        }
        reserved[p.y][p.x] = true;
        /* No proposal targets a snapshot-occupied cell; changing earlier
         * positions cannot create a conflict with another proposed target. */
        r->x = p.x; r->y = p.y; ++w->moves; ++moved;
    }
    arrivals(w);
    ++w->ticks;
    if (!moved && w->completed == before) ++w->stall_ticks;
    else w->stall_ticks = 0;
    if (w->stall_ticks >= 16 && w->completed < (unsigned)w->norders) w->stalled = true;
}
int wh_run(wh_world *w, unsigned threads, unsigned max_ticks) {
    planning_pool pool;
    planning_pool *p = NULL;
    if (!wh_safe(w) || threads > WH_ROBOTS || max_ticks == 0 || max_ticks > 100000)
        return -1;
    if (threads) {
        if (pool_open(&pool,threads)) return -1;
        p = &pool;
    }
    for (unsigned i = 0; i < max_ticks && !w->stalled &&
                           w->completed < (unsigned)w->norders; ++i) {
        tick(w,p);
        if (!wh_safe(w)) {
            if (p) pool_close(p);
            return -1;
        }
    }
    if (p) pool_close(p);
    return w->completed == (unsigned)w->norders ? 0 : 1;
}
bool wh_equal(const wh_world *a, const wh_world *b) {
    if (!a || !b || a->width != b->width || a->height != b->height ||
        a->nrobots != b->nrobots || a->norders != b->norders ||
        a->ticks != b->ticks || a->moves != b->moves ||
        a->waits != b->waits || a->conflicts != b->conflicts ||
        a->completed != b->completed || a->stall_ticks != b->stall_ticks ||
        a->stalled != b->stalled) return false;
    for (int i = 0; i < a->nrobots; ++i)
        if (a->robots[i].x != b->robots[i].x ||
            a->robots[i].y != b->robots[i].y ||
            a->robots[i].order != b->robots[i].order) return false;
    for (int i = 0; i < a->norders; ++i)
        if (a->orders[i].state != b->orders[i].state ||
            a->orders[i].robot != b->orders[i].robot) return false;
    return true;
}
void wh_print(FILE *out, const wh_world *w) {
    if (!out || !w) return;
    for (int y = 0; y < w->height; ++y) {
        for (int x = 0; x < w->width; ++x) {
            char c = w->floor[y][x];
            for (int r = 0; r < w->nrobots; ++r)
                if (w->robots[r].x == x && w->robots[r].y == y) c = (char)('1'+r);
            fputc(c,out);
        }
        fputc('\n',out);
    }
    fprintf(out,"ticks=%u completed=%u/%d moves=%u waits=%u conflicts=%u stalled=%s\n",
            w->ticks,w->completed,w->norders,w->moves,w->waits,w->conflicts,
            w->stalled ? "yes" : "no");
}
