# Task: software prefetch of the next constraint's body states in b3SolveContacts_Convex

Repository: fast3d (Box3D fork), C17. File to modify: `src/contact_solver.c` ONLY.

## The change, exactly

1. Near the top of `src/contact_solver.c`, immediately AFTER the existing `#include` block
   and BEFORE the math comment block, add a portable read-prefetch macro:

   - On GCC/Clang: `__builtin_prefetch( (p), 0, 3 )`
   - Otherwise: a no-op that evaluates nothing.
   - Name it `b3PrefetchRead`. Guard with `#if defined( __GNUC__ ) || defined( __clang__ )`.

2. In `b3SolveContacts_Convex` (provided below in context), immediately AFTER the two
   existing `b3GatherBodies` calls for `bA` and `bB`, insert a prefetch of the NEXT wide
   constraint's body states:

   - Only when `wideIndex + 1 < block.startIndex + block.count`.
   - Let `next = constraints + wideIndex + 1`.
   - For each lane `i` in 0..3: if `next->indexA[i] != 0`, prefetch
     `states + ( next->indexA[i] - 1 )`; same for `next->indexB[i]`.
   - IMPORTANT: skip lanes with index 0. Index 0 means the null body (a shared global
     read-only identity state); we must not pull that shared cache line per-iteration.
   - Write it as a small plain `for ( int i = 0; i < 4; ++i )` loop inside the guard.

## Hard constraints

- Do NOT touch any floating-point arithmetic, any b3FloatW math, or any control flow of
  the existing solve. Prefetch must be the ONLY behavioral addition (it changes no
  results, that is the entire point: this must preserve cross-platform determinism).
- Do NOT modify b3GatherBodies, b3ScatterBodies, or any other function.
- Match the file style: tabs for indentation, spaces inside parens `if ( x )`, braces on
  their own lines.
- Minimal diff. No comments except one short line above the prefetch block saying it
  hides the scattered gather latency of the next constraint.

CONTEXT: src/contact_solver.c:1-30
CONTEXT: src/contact_solver.c:1637-1700
