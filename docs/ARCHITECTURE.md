# Architecture

The parser takes exactly N² single-character symbols for N = 4, 9, or 16 and rejects unsupported characters, excess, and missing cells. Each cell is an unsigned byte. A blank is parsed but makes completed-board validation fail.

`sudoku_unit_valid` tests all N values of one row, column or box with a bounded bitset. It rejects values outside 1..N *before* shifting. The three result arrays give one slot per constraint and are initialized before execution; aggregation requires every result to pass. The box origin for index i is row = (i / box) * box, col = (i % box) * box, with box = sqrt(N).

**Sequential:** call each task directly.
**Original:** spawn N+2 pthreads: one covers all rows, another covers all columns and N cover each subgrid.
**Fine:** spawn 3N pthreads, one for each constraint.
**Pool:** spawn 1..256 pthreads with a mutex guarding the next-task index. Each completed task writes only its designated result slot; input is shared but read-only.

All workers complete before the calling thread aggregates results. Partially successful creation joins every started thread before returning an error. The pool's mutex and thread storage are scoped to one validation invocation, so this is not a persistent cross-invocation thread pool. Timed batch throughput and persistent-worker architectures are possible future extensions.

Exit code 1 signifies an actually invalid/incomplete board; exit code 2 means the attempted check was not completed (including a thread creation failure). Neither parse errors nor missing files are treated as Sudoku invalidity.

The original 2017 file is intentionally not part of the new build. No source changes to it are required to run the modern application.
