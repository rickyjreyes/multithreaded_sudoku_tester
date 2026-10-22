# Cross-domain swarm laboratory

This is a **public, synthetic and non-proprietary** experimentation framework for parallel update algorithms. It is deliberately not restricted to warehouses. A shared scheduler runs several small, explicitly simplified dynamical systems. This is not validated bee ethology, a realistic traffic-network model, verified particle physics, a biological prediction, or a safety-certified controller.

## Shared experimental structure

For a given seeded initial state, at each discrete tick all independent elements read the **same previous-state snapshot**, and write only their own index in a next-state buffer. Thread scheduling never alters the order of arithmetic *within one element*. No worker modifies the shared current state. Only after successful joins does the caller publish next state and compute summary metrics in a fixed index order.

- sequential: update 0..N-1 in the calling thread
- static: fixed contiguous partitions across 1..32 pthreads
- atomic: atomic fetch-and-add claims the next index
- mutex: mutex-protected shared next-index counter

Threads are created/joined at **each tick**, not persistent between ticks. This is intentional to make per-step overhead visible and keeps this change separate from a long-lived pool experiment.

The *compare* mode runs all four schedulers from identical initial states, checking bit-for-bit equality of the entire evolving state and summary metrics **after each tick**, within the same binary/platform. `digest` is a noncryptographic state fingerprint for reporting, not a substitute for comparing full state. Floating-point libm implementations and compiler flags may change bit patterns between platforms; cross-platform statistical equivalence requires a separate protocol.

## Explicit toy models

### Bees (`--model bees`)

N periodic-ring nodes carry states undecided=0, committed-to-A=1, committed-to-B=2. Initial assignments are deterministic from seed with forced A and B seeds. Previously committed nodes remain committed; undecided nodes inspect their two old-state neighbors. With at least one committed neighbor, the node commits with 60% probability chosen by a deterministic hash of (seed, tick, index), adopting the majority neighbor choice or hash-tiebreaking equally. Otherwise it remains undecided. This is **not** a scientifically calibrated honeybee recruitment model. Primary metric: committed fraction; secondary: absolute A/B fraction imbalance. Commitment cannot decrease.

### Traffic (`--model traffic`)

Periodic one-dimensional binary occupancy with approximately 35% initial density. Parallel elementary cellular automaton **rule 184** moves each car one cell right only if the next cell in the old snapshot is empty. A cell stays occupied if its occupant is blocked, or becomes occupied if empty with an occupied predecessor. There are no intersections, heterogeneous drivers, or road maps. Primary metric: fraction of cells with an old-state car that can advance; secondary: density. Invariant: total car count is conserved.

### Phase (`--model phase`)

N oscillator phases in [0, 2π), initialized by seeded values. A forward-Euler time step of 0.05 uses each oscillator's fixed seeded natural frequency in [-0.12, 0.12] and all-to-all sinusoidal coupling K/N times the sum over other phases; K defaults to 1.5 and is selectable with `--coupling 0..8`. This is **Kuramoto-like**, not a validated physical synchronization model. Primary metric: phase order parameter |mean(exp(i phase))|; secondary: 1 - order parameter. Each oscillator processes neighbors in the same fixed index order, independently of scheduler.

### Life (`--model life`)

Conway's Game of Life, an N-cell toroidal square with 8 Moore neighbors, binary alive/dead cells initialized at approximately 30% alive. A live cell survives with two or three neighbors, a dead cell is born with three. Primary metric: live-cell fraction; secondary: fraction of cells that flipped on the last tick. N must be a perfect square (for example 16, 64, 144, 256). This is a classic example of emergent patterns from local rules, not a biological organism model.

## Commands

~~~sh
make all test
./swarm-lab run --model bees --agents 128 --steps 40 --threads 4 --scheduler static --seed 42
./swarm-lab compare --model traffic --agents 128 --steps 50 --threads 8
./swarm-lab compare --model phase --agents 64 --steps 30 --threads 8 --coupling 1.5
./swarm-lab compare --model life --agents 256 --steps 40 --threads 8
./swarm-lab benchmark --model life --agents 256 --steps 30 --threads 4 --trials 7
~~~

`run` reports the final metric, secondary metric and digest. `compare` returns nonzero at the **first** scheduler/tick disagreement. `benchmark` reports median wall time, steps/s, identical-state verification, and CSV-compatible records for all four schedulers. A warm-up and full correctness verification occur outside the timer. Each timed run includes thread launch/join at every step and metric/digest computation. Results must not be advertised as performance wins without platform details, independent trials and uncertainty analysis.

## Tests, interpretive boundaries, extensions

The tests cover repeatable seeds and thread counts, all four models, full state/digest agreement at every tick, invalid configurations, car conservation, monotonic commitment, oscillator phase bounds, and a two-step Life blinker. CI checks GCC and Clang plus CLI smoke.

Future independent public modules could add Vicsek alignment, graph-based contagion, excitable media, reaction-diffusion, flocking, coupled-map lattices, stochastic sensitivity, phase transitions, interaction-topology experiments, bounded persistent pools, and ensemble statistics. Add actual model science and literature separately: do not conflate a visual resemblance to bees/cars/organisms with empirical validation.

**Scope rule:** no real warehouse architecture, fleet control, device/actuator interfaces, operational telemetry, private company data, proprietary rules or implementation. The historical Sudoku file is untouched. The warehouse-specific PR #2 was closed without merge; public history remains visible and closing it does not erase GitHub records.
