# Original 2017 project: retained unchanged

[sudoku_rickyreyes_794.c](../sudoku_rickyreyes_794.c) is the original single-file coursework implementation. Its design uses pthreads to validate a hard-coded completed 9x9 grid: a row worker, a column worker, and nine subgrid workers. It has historical and educational value, but its results are not safe to trust or benchmark unmodified.

Known defects of the historical file (which the modern implementation does **not** inherit):

- The parameter initializer uses \`i <= GRID_SIZE\` and writes past nine pointer slots.
- Subgrid coordinate mapping mixes zero-based worker indexes with one-based square indexes, omitting a subgrid.
- The final aggregation accepts any one passing subgrid instead of requiring all nine.
- Values are used as array indexes without checking range 1-9.
- Parameter allocations are leaked; pthread creation errors are ignored.
- Boolean status is encoded in void-pointer thread returns instead of stable result storage.

The corrected implementation lives under \`src/\` and \`include/\`. It uses the same basic 11-worker architecture as one of four independently selectable modes, but keeps the original file byte-for-byte unmodified.

This is a **validator**, not a solver. A completed Latin square may pass all rows/columns but still fail every 3x3 subgrid; an incomplete grid is not accepted.
