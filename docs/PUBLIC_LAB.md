# Public concurrency laboratory: scope and measurement

This repository is a **generic, public test harness**. It does not represent a robot-control architecture, an actual warehouse, or any private project implementation. Keep production code, facility layouts, company data, control policies, telemetry, credentials, private algorithms and device connections outside it. Experiments use only synthetic indexed arithmetic jobs and published Sudoku rules. PR #2 was closed without merging after the scope was clarified; its historical PR remains publicly visible.

## What the synthetic workload represents

A configuration sets:
- Number of independent jobs (1–65,536).
- Arithmetic work per job (1–100,000 rounds).
- Deterministic unsigned 64-bit seed.
- Uniform profile, or skewed profile with every eighth job taking eight times the work.
- One to 32 worker threads.

A deliberately simple noncryptographic mixer generates the result for each job by index; there are no coordinates, agents, orders, operational variables, I/O requests or input files. The mixer is a repeatable computational workload, not a claim of cryptographic strength or a production-quality random number generator.

All modes write to *disjoint result slots*. The sequential reference and every concurrent mode must agree **byte-for-byte on the entire ordered result vector**. A digest is printed for quick reproducibility but is not used as a substitute for full comparison.

## Scheduling strategies

| Mode | Description |
| --- | --- |
| sequential | Execute every job on the caller thread |
| static | Split contiguous job ranges into one range per pthread |
| atomic | Workers claim a shared job index using relaxed atomic fetch-and-add |
| mutex | Workers claim a shared job index under a mutex |

These deliberately isolate scheduling overhead and load balancing. Threads are created and joined for each run; unlike a production service, **no worker pool persists across separate runs**. Thread ordering is uncontrolled, while the result vector is deterministic. There is no shared mutable per-job result and no private architecture.

## Usage

~~~sh
make all test
./concurrency-lab run --jobs 512 --work 500 --threads 4 --mode atomic
./concurrency-lab compare --jobs 257 --work 200 --threads 8 --profile skewed --seed 42
./concurrency-lab benchmark --jobs 8192 --work 1000 --threads 8 --profile uniform --trials 7
~~~

The benchmark runs one untimed warmup and then measures each invocation with CLOCK_MONOTONIC, including thread creation/join and ordered digest reduction. Full result-vector comparisons occur outside the timing window. Its CSV-compatible output reports the median of independent trial durations and throughput derived from that median. A fixed strategy order, process/CPU scheduling, thermal variation, CPU affinity, and OS background load can bias measurements; compare several independent benchmark invocations, document machine/compiler/OS, and do not infer production performance from these synthetic numbers.

The original single-file Sudoku coursework remains unchanged. Its separate corrected validator is retained; the new lab does not depend on or modify the puzzle format. See docs/ORIGINAL_PROJECT.md and docs/ARCHITECTURE.md. Changes to the scope of any private system must be made in a separate private repository.
