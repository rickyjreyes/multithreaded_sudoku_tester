# Benchmarking guide

Run \`make && ./sudoku-tester benchmark --iterations 1000 --format csv\` to measure each validation strategy on the same board. The CLI uses CLOCK_MONOTONIC wall time around N iterations for each mode, reports elapsed milliseconds, mean microseconds per board, boards/second, and speedup vs sequential. It checks full per-constraint equivalence against the baseline **on every iteration**. With no input, it uses the historical 9x9 sample or a constructed valid 4x4/16x16 board selected by --size.

Caveats: pthread creation/join happens *inside* every measured iteration, including pool mode, so these figures measure end-to-end per-call overhead rather than steady-state worker-pool throughput. The sequential workload is tiny; more threads are likely slower for a single 9x9 validation. Other hosts and loads change results. Run several independent samples, disclose CPU/compiler/OS, report distributions (not just a lucky fastest run) and make comparisons using a fixed compiler and input.

Example:

~~~sh
make
./sudoku-tester benchmark --iterations 1000 --format csv > results.csv
./sudoku-tester benchmark examples/invalid.sdk --iterations 500
./sudoku-tester benchmark --size 16 --threads 8 --iterations 500
~~~

\`results.csv\` is ignored by git by default. There are no prerecorded or fabricated measurements in this repository.
