# Warehouse Hive — Parallel Coordination Laboratory

A C / POSIX-threads research playground for **concurrent warehouse robot coordination**. The original 2017 Sudoku coursework file, [sudoku_rickyreyes_794.c](sudoku_rickyreyes_794.c), is preserved **byte-for-byte** as the historical origin of the project. Its updated Sudoku validator also remains available as a separate CLI. This repository now explores shared-grid scheduling, parallel planning, conflicting reservations, liveness and reproducible performance.

**Simulation only.** No physical robot interfaces, certified collision avoidance, real-time guarantee, or claim of deadlock freedom. See [the warehouse architecture and limitations](docs/WAREHOUSE.md).

## Build

Requires a POSIX environment, C11 compiler, pthreads and make:

~~~sh
make
make test
./warehouse-sim demo --threads 4 --ticks 250
./warehouse-sim run examples/warehouse_demo.wh --threads 4 --ticks 250
./warehouse-sim run examples/warehouse_demo.wh --threads 0 --format json
./warehouse-sim compare examples/warehouse_demo.wh --threads 4
./warehouse-sim benchmark examples/warehouse_demo.wh --threads 4 --iterations 10
./warehouse-sim run examples/warehouse_conflict.wh --threads 2
~~~

Warehouse modes: threads=0 executes route proposals sequentially; 1..8 enables a **persistent pool of planning workers for the entire simulation run**. The hive supervisor commits all movement in deterministic robot-ID order after a planning barrier. All modes use the same occupancy snapshot and route planner. Comparison checks full final world state, robot positions, order states and counters; benchmark checks agreement before reporting throughput.

### Scenario format

~~~text
SIZE 7 5
MAP
#######
#1.2..#
#.....#
#.....#
#######
ORDERS
2 1 1 2
2 1 4 2
~~~

Map: \`#\` wall, \`.\` floor, \`A\` aisle, \`H\` home, \`P\` pickup, \`1..8\` robots. Orders: pickup x/y and delivery x/y, zero-based. Robot IDs must start at 1 and be contiguous. Valid endpoints must be walkable. CLI: \`./warehouse-sim --help\`.

The conservative supervisor rejects a move into any cell occupied in the tick's initial snapshot or already reserved by a smaller-ID robot. This prevents duplicate occupancy and edge swaps but **does not guarantee progress**: sixteen nonproductive ticks trigger a stall report. Future extensions can introduce time-expanded reservations, fairness and safe deadlock recovery. This is a digital experiment, not a deployment controller.

Warehouse exit status: 0 all orders complete (or comparisons match); 1 simulation unfinished due to budget or stall; 2 invalid invocation, I/O, parsing or worker/safety error. A comparison can correctly report MATCH for two identical unfinished runs.

## Original Sudoku track (retained)

The historical file [sudoku_rickyreyes_794.c](sudoku_rickyreyes_794.c) remains unchanged and is **not** the trusted executable, since it has known defects detailed in [ORIGINAL_PROJECT](docs/ORIGINAL_PROJECT.md).

The modern \`sudoku-tester\` executable remains operational:

~~~sh
./sudoku-tester validate examples/valid.sdk
./sudoku-tester validate examples/invalid.sdk --format json
./sudoku-tester validate examples/valid.sdk --mode sequential
./sudoku-tester validate examples/valid.sdk --mode 11
./sudoku-tester validate examples/valid.sdk --mode 27
./sudoku-tester validate examples/valid.sdk --mode pool --threads 4
./sudoku-tester batch examples/
./sudoku-tester benchmark --iterations 1000 --format csv
~~~

Supports completed 4x4/9x9/16x16 Sudoku validation, detailed constraint failures and benchmarking; it does not solve Sudoku puzzles. See [the Sudoku architecture](docs/ARCHITECTURE.md) and [benchmark notes](benchmarks/README.md).

## Verification

\`make test\` runs both Sudoku and Warehouse suites. \`make sanitize\` and \`make tsan\` target Address/UndefinedBehaviorSanitizer and ThreadSanitizer where supported. GitHub Actions runs GCC and Clang build/tests and CLI smoke checks. Benchmark results depend on hardware; no synthetic figures are advertised as measurements.

No license has been added: licensing remains the repository owner's choice.
