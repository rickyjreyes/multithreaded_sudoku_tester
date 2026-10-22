# Multithreaded Sudoku Tester

A modern C / POSIX threads constraint validator built alongside the original **2017 coursework exercise**. The historical file [sudoku_rickyreyes_794.c](sudoku_rickyreyes_794.c) is **preserved verbatim**. Do not build/run it as the trusted checker: its documented bounds, subgrid-indexing and result-aggregation bugs remain intentionally available for historical comparison. See [the original-project notes](docs/ORIGINAL_PROJECT.md).

The modern program checks **completed boards**, not whether an incomplete puzzle has a solution. It supports 4x4, 9x9 and 16x16, gives per-row/column/box diagnostics, and compares sequential validation against POSIX concurrency strategies. It does **not** claim multithreading is faster for one small board.

## Build and run

Requires a POSIX platform with a C11 compiler, pthreads and make:

~~~sh
make
make test
./sudoku-tester validate examples/valid.sdk
./sudoku-tester validate examples/invalid.sdk --format json
./sudoku-tester validate examples/valid.sdk --mode sequential
./sudoku-tester validate examples/valid.sdk --mode 11
./sudoku-tester validate examples/valid.sdk --mode 27
./sudoku-tester validate examples/valid.sdk --mode pool --threads 4
./sudoku-tester batch examples/
./sudoku-tester benchmark --iterations 1000
./sudoku-tester benchmark --iterations 1000 --format csv
~~~

**Exit codes:** 0 valid/success; 1 invalid or incomplete board (or batch with such boards); 2 invocation, parse, I/O or threading error. Thus the supplied mixed batch fixture intentionally exits 1. CLI help: \`./sudoku-tester --help\`.

Input is exactly N×N cells (N selected by \`--size 4|9|16\`, default 9), using \`1..9\`, then \`A..G\` for 10..16. Zero and dot mean blank, which makes validation fail as *incomplete*. Compact lines or separated symbols work. Whitespace, \`| + - , :\` are separators; \`#\` starts a comment. Each file describes one board. Batch mode processes \`.sdk\` files in a directory, nonrecursively. To try larger sizes, use \`--size 4\` or \`--size 16\` with a matching board. The benchmark uses the original 9×9 sample by default or a constructed valid board for the other sizes if no file is given.

## Architectures

| Mode | Validation work | 9x9 worker count |
| --- | --- | ---: |
| sequential | Same checks on calling thread | 0 |
| 11 / original | All rows, all columns, one task per box | 11 |
| 27 / fine | One task per row, column and box | 27 |
| pool | One task per constraint, mutex-protected task queue | Configurable (default 4) |

The names 11 and 27 refer to the original **9×9** experiment; on N×N boards the respective worker counts are N+2 and 3N. All modes run the *same checking function*, and the benchmark fails if their complete result vectors disagree. Read-only board memory is shared; results use separate slots, avoiding result data races. Each invocation creates/joins its workers; the "pool" reuses a bounded set of threads **within an invocation**, not across separate validation calls. These details matter when interpreting performance numbers.

## Quality gates

~~~sh
make test       # parser, 4/9/16 grids, invalid/incomplete grids, mode agreement
make sanitize   # AddressSanitizer and UndefinedBehaviorSanitizer (supported hosts)
make tsan       # ThreadSanitizer (requires compatible platform/runtime)
make clean
~~~

GitHub Actions builds with GCC and Clang, runs the unit tests and smoke checks. Sanitizer targets are available for local use; actual measurements and sanitizer availability depend on host hardware/toolchain.

For design and benchmarking methodology, see [ARCHITECTURE](docs/ARCHITECTURE.md) and [BENCHMARKING](benchmarks/README.md).

No license was added: repository licensing remains the owner's decision.
