#include "swarm.h"
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

static const double TWO_PI = 6.283185307179586476925286766559;

static uint64_t mix(uint64_t x) {
    x ^= x >> 30; x *= UINT64_C(0xbf58476d1ce4e5b9);
    x ^= x >> 27; x *= UINT64_C(0x94d049bb133111eb);
    return x ^ (x >> 31);
}
static double unit_random(uint64_t x) {
    return (double)(mix(x) >> 11) * 0x1.0p-53;
}
static uint64_t random_key(const swarm_world *w, size_t index, uint64_t salt) {
    return w->seed ^ ((uint64_t)(index + 1) * UINT64_C(0x9e3779b97f4a7c15)) ^
           ((uint64_t)(w->tick + 1) * UINT64_C(0xd1b54a32d192ed03)) ^ salt;
}
const char *swarm_model_name(swarm_model model) {
    switch (model) {
        case SWARM_BEES: return "bees";
        case SWARM_TRAFFIC: return "traffic";
        case SWARM_PHASE: return "phase";
        case SWARM_LIFE: return "life";
        default: return "unknown";
    }
}
const char *swarm_schedule_name(swarm_schedule schedule) {
    switch (schedule) {
        case SWARM_SEQUENTIAL: return "sequential";
        case SWARM_STATIC: return "static";
        case SWARM_ATOMIC: return "atomic";
        case SWARM_MUTEX: return "mutex";
        default: return "unknown";
    }
}
const char *swarm_metric_name(swarm_model model) {
    switch (model) {
        case SWARM_BEES: return "committed_fraction";
        case SWARM_TRAFFIC: return "flow_per_cell";
        case SWARM_PHASE: return "phase_order";
        case SWARM_LIFE: return "alive_fraction";
        default: return "unknown";
    }
}
const char *swarm_secondary_name(swarm_model model) {
    switch (model) {
        case SWARM_BEES: return "choice_imbalance";
        case SWARM_TRAFFIC: return "car_density";
        case SWARM_PHASE: return "phase_variance_proxy";
        case SWARM_LIFE: return "changed_fraction";
        default: return "unknown";
    }
}
static void metrics(swarm_world *w, const double *prior) {
    size_t n = w->count;
    double primary = 0.0, secondary = 0.0;
    switch (w->model) {
        case SWARM_BEES: {
            size_t a = 0, b = 0;
            for (size_t i = 0; i < n; ++i) {
                a += w->state[i] == 1.0;
                b += w->state[i] == 2.0;
            }
            primary = (double)(a + b) / (double)n;
            secondary = fabs((double)a - (double)b) / (double)n;
            break;
        }
        case SWARM_TRAFFIC: {
            size_t moved = 0, cars = 0;
            for (size_t i = 0; i < n; ++i) {
                cars += w->state[i] == 1.0;
                moved += prior[i] == 1.0 && prior[(i + 1) % n] == 0.0;
            }
            primary = (double)moved / (double)n;
            secondary = (double)cars / (double)n;
            break;
        }
        case SWARM_PHASE: {
            double sx = 0.0, sy = 0.0;
            for (size_t i = 0; i < n; ++i) {
                sx += cos(w->state[i]);
                sy += sin(w->state[i]);
            }
            primary = hypot(sx, sy) / (double)n;
            secondary = 1.0 - primary;
            break;
        }
        case SWARM_LIFE: {
            size_t alive = 0, changed = 0;
            for (size_t i = 0; i < n; ++i) {
                alive += w->state[i] == 1.0;
                changed += prior[i] != w->state[i];
            }
            primary = (double)alive / (double)n;
            secondary = (double)changed / (double)n;
            break;
        }
        default: break;
    }
    w->metric = primary;
    w->secondary = secondary;
}
int swarm_init(swarm_world *w, swarm_model model, size_t n,
               uint64_t seed, double coupling) {
    if (!w || model < SWARM_BEES || model > SWARM_LIFE ||
        n < 4 || n > SWARM_CAPACITY || !isfinite(coupling) ||
        coupling < 0.0 || coupling > 8.0) return -1;
    unsigned side = 0;
    if (model == SWARM_LIFE) {
        for (unsigned i = 2; i <= 16; ++i)
            if ((size_t)i * i == n) side = i;
        if (!side) return -1;
    }
    memset(w, 0, sizeof(*w));
    w->model = model; w->count = n; w->side = side;
    w->seed = seed; w->coupling = coupling;
    for (size_t i = 0; i < n; ++i) {
        double r = unit_random(random_key(w, i, 0));
        if (model == SWARM_BEES)
            w->state[i] = r < 0.07 ? 1.0 : r < 0.14 ? 2.0 : 0.0;
        else if (model == SWARM_TRAFFIC)
            w->state[i] = r < 0.35 ? 1.0 : 0.0;
        else if (model == SWARM_PHASE)
            w->state[i] = TWO_PI * r;
        else
            w->state[i] = r < 0.30 ? 1.0 : 0.0;
    }
    if (model == SWARM_BEES) {
        w->state[0] = 1.0;
        w->state[n / 2] = 2.0;
    }
    metrics(w, w->state);
    return 0;
}
static double next_value(const swarm_world *w, size_t i) {
    const size_t n = w->count;
    const double *s = w->state;
    if (w->model == SWARM_BEES) {
        if (s[i] != 0.0) return s[i];
        int a = (s[(i + n - 1) % n] == 1.0) +
                (s[(i + 1) % n] == 1.0);
        int b = (s[(i + n - 1) % n] == 2.0) +
                (s[(i + 1) % n] == 2.0);
        if (!a && !b) return 0.0;
        double r = unit_random(random_key(w,i,UINT64_C(0x0ca8f38d7b1e)));
        if (r >= 0.60) return 0.0;
        if (a > b) return 1.0;
        if (b > a) return 2.0;
        return unit_random(random_key(w,i,UINT64_C(0xe5cab42f19))) < 0.5 ?
               1.0 : 2.0;
    }
    if (w->model == SWARM_TRAFFIC) {
        /* Periodic elementary CA 184: occupied cells move right into a
         * vacant neighbor in parallel; car count is conserved. */
        return s[i] == 1.0 ?
               (s[(i + 1) % n] == 1.0 ? 1.0 : 0.0) :
               (s[(i + n - 1) % n] == 1.0 ? 1.0 : 0.0);
    }
    if (w->model == SWARM_PHASE) {
        double influence = 0.0, angle = s[i];
        for (size_t j = 0; j < n; ++j)
            influence += sin(s[j] - angle); /* fixed deterministic j order */
        uint64_t raw = mix(w->seed + (uint64_t)i);
        int offset = (int)(raw % 2001u) - 1000;
        double omega = 0.12 * (double)offset / 1000.0;
        double next = angle + 0.05 *
            (omega + w->coupling * influence / (double)n);
        next = fmod(next, TWO_PI);
        return next < 0.0 ? next + TWO_PI : next;
    }
    unsigned side = w->side;
    int row = (int)(i / side), col = (int)(i % side), neighbors = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int y = (row + dy + (int)side) % (int)side;
            int x = (col + dx + (int)side) % (int)side;
            neighbors += s[(size_t)y * side + (size_t)x] == 1.0;
        }
    return neighbors == 3 || (s[i] == 1.0 && neighbors == 2) ? 1.0 : 0.0;
}

typedef struct {
    const swarm_world *world;
    double *next;
    swarm_schedule schedule;
    atomic_size_t atomic_next;
    size_t mutex_next;
    atomic_int failed;
    pthread_mutex_t mutex;
} shared;
typedef struct {
    shared *shared;
    size_t first, end;
} task;

static void *update_worker(void *ptr) {
    task *arg = ptr;
    shared *s = arg->shared;
    if (s->schedule == SWARM_STATIC) {
        for (size_t i = arg->first; i < arg->end; ++i)
            s->next[i] = next_value(s->world, i);
        return NULL;
    }
    for (;;) {
        size_t i;
        if (s->schedule == SWARM_ATOMIC) {
            i = atomic_fetch_add_explicit(&s->atomic_next, 1,
                                          memory_order_relaxed);
        } else {
            if (pthread_mutex_lock(&s->mutex) != 0) {
                atomic_store(&s->failed, 1); break;
            }
            i = s->mutex_next++;
            if (pthread_mutex_unlock(&s->mutex) != 0) {
                atomic_store(&s->failed, 1); break;
            }
        }
        if (i >= s->world->count) break;
        s->next[i] = next_value(s->world, i);
    }
    return NULL;
}

int swarm_step(swarm_world *w, swarm_schedule schedule, unsigned threads) {
    if (!w || w->count < 4 || w->count > SWARM_CAPACITY ||
        w->model < SWARM_BEES || w->model > SWARM_LIFE ||
        schedule < SWARM_SEQUENTIAL || schedule > SWARM_MUTEX ||
        threads < 1 || threads > SWARM_MAX_THREADS || w->tick == UINT32_MAX)
        return -1;
    if (w->model == SWARM_LIFE && (w->side < 2 ||
        (size_t)w->side * w->side != w->count)) return -1;

    double next[SWARM_CAPACITY];
    if (schedule == SWARM_SEQUENTIAL) {
        for (size_t i = 0; i < w->count; ++i)
            next[i] = next_value(w,i);
    } else {
        shared s;
        pthread_t ids[SWARM_MAX_THREADS];
        task tasks[SWARM_MAX_THREADS];
        unsigned started = 0;
        int failed = 0;
        memset(&s,0,sizeof(s));
        s.world = w; s.next = next; s.schedule = schedule;
        atomic_init(&s.atomic_next, 0);
        atomic_init(&s.failed,0);
        bool locking = schedule == SWARM_MUTEX;
        if (locking && pthread_mutex_init(&s.mutex,NULL) != 0) return -1;
        for (unsigned k = 0; k < threads; ++k) {
            tasks[k] = (task){.shared=&s,
                .first=(w->count * k) / threads,
                .end=(w->count * (k + 1u)) / threads};
            if (pthread_create(&ids[k],NULL,update_worker,&tasks[k]) != 0) {
                failed = 1; break;
            }
            ++started;
        }
        for (unsigned k = 0; k < started; ++k)
            if (pthread_join(ids[k],NULL) != 0) failed = 1;
        if (locking && pthread_mutex_destroy(&s.mutex) != 0) failed = 1;
        if (atomic_load(&s.failed)) failed = 1;
        if (failed) return -1;
    }
    double prior[SWARM_CAPACITY];
    memcpy(prior,w->state,w->count * sizeof(double));
    memcpy(w->state,next,w->count * sizeof(double));
    ++w->tick;
    metrics(w,prior);
    return 0;
}

bool swarm_equal(const swarm_world *a, const swarm_world *b) {
    return a && b && a->model == b->model && a->count == b->count &&
           a->side == b->side && a->tick == b->tick &&
           a->seed == b->seed &&
           memcmp(&a->coupling,&b->coupling,sizeof(double)) == 0 &&
           memcmp(a->state,b->state,a->count * sizeof(double)) == 0 &&
           memcmp(&a->metric,&b->metric,sizeof(double)) == 0 &&
           memcmp(&a->secondary,&b->secondary,sizeof(double)) == 0;
}
uint64_t swarm_digest(const swarm_world *w) {
    if (!w || w->count > SWARM_CAPACITY) return 0;
    uint64_t hash = mix(w->seed ^ (uint64_t)w->tick ^
                        ((uint64_t)w->model << 48) ^ (uint64_t)w->count);
    for (size_t i = 0; i < w->count; ++i) {
        uint64_t bits = 0;
        memcpy(&bits,&w->state[i],sizeof(bits));
        hash = mix(hash ^ bits ^ (uint64_t)i);
    }
    return hash;
}
