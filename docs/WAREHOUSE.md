# Warehouse Hive: prototype design and limits

This is a deterministic discrete-grid simulator for coordinated mobile robots, inspired by an autonomous multithreaded warehouse. It does **not** command hardware or certify physical robot safety. Sudoku remains the historical concurrency exercise; the warehouse introduces dynamic shared state and resource conflicts.

## World and workload

- 3..32 wide/tall rectangular grid; '#' wall, '.' floor, 'A' aisle, 'H' home, 'P' pickup markers, 1..8 distinct robots.
- A scenario file starts with SIZE width height, then MAP, exactly height map lines, ORDERS and up to 128 pickup_x pickup_y delivery_x delivery_y lines (all zero-based). Robot IDs start at 1 and must be contiguous. Floor markers do not impose one-way driving rules yet.
- FIFO order assignment to idle robots in ascending robot ID; a robot moves toward pickup, then delivery. Deliveries increment the completed counter.
- Each tick: finish arrivals and assign jobs; concurrently compute shortest-path **proposals from an immutable snapshot**; a single hive supervisor applies nonconflicting moves in ascending robot ID; finalize arrivals and validate the shared state.
- Deterministic BFS tie order is north, east, south, west. Other robots' *snapshot cells* are blocked during path planning. Only presently unoccupied cells can be targeted. A reservation rejects competing proposals to the same cell; lower robot IDs have priority. As a consequence swaps and moving into vacated snapshot cells are deliberately forbidden. It is conservative, not an optimized multi-agent pathfinder.
- No dynamic allocation during ticks; the pthread worker pool is created once per wh_run and reused across all ticks in that run. threads=0 executes exactly the same proposals sequentially.
- The supervisor, not robot threads, mutates robot positions and task states. Worker writes are to disjoint proposal slots; mutex + condition variables coordinate lifecycle. Safe states require in-bounds walkable robots and no overlapping positions.
- After 16 consecutive ticks without movement or task completion, the simulator reports stalled. This detects but does NOT prevent all deadlocks or guarantee liveness. Bottlenecks, capacity constraints, fairness/starvation, dynamic obstacles, sensor faults, and real-world braking remain open extensions.

## Experiments and statuses

Run warehouse-sim demo, warehouse-sim run examples/warehouse_demo.wh, warehouse-sim compare examples/warehouse_demo.wh --threads 4, warehouse-sim benchmark examples/warehouse_demo.wh --iterations 10 --threads 4. compare checks every final robot, order status, and counters against the sequential reference and reports MATCH; benchmark also checks agreement. The benchmark includes thread-pool creation and teardown for each independent *trial*, while threads persist across ticks within a trial. It is not a proof of real-time deadline compliance.

Output provides ticks, completed orders, moves, waits, contested reservations, and stalled status. Exit 0 all orders complete or valid comparison; 1 unfinished by tick budget or stall detector; 2 errors. The original Sudoku CLI and its tests still build independently.

## Expansion path

Add time-expanded cooperative route planning, reservation horizons, intersection permissions, dynamic arrival of orders, priorities/aging, workload scaling, conflict/deadlock scenarios, property-based and adversarial tests, latency distributions, persistent cross-scenario worker service, and separately validated experimental dispatch/routing policies. Evaluate correctness separately from speed, and compare any experimental policy against locked baselines under identical scenario seeds. No claim about physical safety follows from grid simulation.
