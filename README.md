# fast3d

**Box3D, but faster.**

> ## ⚠️ DO NOT USE THIS LIBRARY
>
> This repository exists for exactly one reason: to answer the question, can we
> throw Claude Code at Box3D and make it faster? The answer is yes. But
> [Erin Catto](https://github.com/erincatto) is much smarter than us, and any good
> optimizations found here will be ported back into vanilla
> [Box3D](https://github.com/erincatto/box3d) — so you should just use that.

## What is this?

fast3d is a fork of [Box3D](https://github.com/erincatto/box3d) used as a test bed
for one experiment: point [Claude Code](https://claude.com/claude-code) at a mature,
carefully optimized physics engine and see whether it can find real speedups.

It can. The catch is that the biggest single win comes from giving up cross platform
determinism, which Box3D guarantees deliberately.

## What changed?

1. **`-ffp-contract=fast`** — Box3D pins floating point contraction off for
   [cross platform determinism](https://box2d.org/posts/2024/08/determinism/).
   fast3d trades that determinism for fused multiply-add. Controlled by the
   `BOX3D_GO_FAST` CMake option, default `ON`.
2. **Link time optimization** — on by default in release builds.
3. **SIMD Gauss map edge rejection** *(retired)* — fast3d's `b3QueryEdgeDirections`
   tested four hull edge pairs per iteration (NEON and SSE2), and was most of the win
   on hull-heavy scenes. Box3D has since shipped its own
   [SIMD hull collision](https://github.com/erincatto/box3d/pull/93) that vectorizes
   the whole pipeline over SoA hull data. That work is ported here and supersedes the
   fast3d kernel — the benchmark tables below predate it on both sides.
4. **Transposed body gathers** — the contact solver gathers body state with vector
   loads and 4x4 transposes instead of building lanes one float at a time.
5. **NEON support vertex scan** — four vertices per iteration with `vld3q` deinterleave.
6. **`b3BodyState` padded to 64 bytes** — resolving an existing `todo_erin` in the source.

## Benchmarks

The benchmark suite that ships with Box3D. Stock Box3D and fast3d are each built with
their default release settings and interleaved per scene so neither side gets the warm
half of the machine. M3 Ultra: 8 workers, minimum of 4 runs. EPYC: 1 worker, minimum
of 2 runs.

### Apple M3 Ultra (NEON), 8 workers

| benchmark | Box3D (ms) | fast3d (ms) | speedup |
| --- | ---: | ---: | ---: |
| convex_pile | 7657 | 2576 | **2.97x** |
| junkyard | 2925 | 2245 | 1.30x |
| trees50 | 111 | 98 | 1.14x |
| large_pyramid | 348 | 307 | 1.13x |
| many_pyramids | 308 | 271 | 1.13x |
| joint_grid | 163 | 147 | 1.11x |
| washer | 4239 | 3854 | 1.10x |
| trees25 | 183 | 173 | 1.06x |
| rain | 413 | 396 | 1.04x |
| trees100 | 88 | 85 | 1.03x |
| large_world | 20 | 21 | wash* |

*large_world runs in ~20 ms and flips winner run to run.

### AMD EPYC 9124 (SSE2), 1 worker

The OS on this machine is confined to a single core (the rest belong to game servers),
so this is the single threaded comparison.

| benchmark | Box3D (ms) | fast3d (ms) | speedup |
| --- | ---: | ---: | ---: |
| convex_pile | 62947 | 25326 | **2.49x** |
| junkyard | 32650 | 27476 | 1.19x |
| trees25 | 1035 | 893 | 1.16x |
| trees50 | 463 | 406 | 1.14x |
| washer | 42506 | 40904 | 1.04x |
| trees100 | 272 | 266 | 1.02x |
| many_pyramids | 4366 | 4324 | 1.01x |
| large_pyramid | 3423 | 3432 | 1.00x |
| joint_grid | 1821 | 1857 | 0.98x |
| rain | 3102 | 3162 | 0.98x |
| large_world | 11 | 11 | wash |

joint_grid and rain give back about 2% on x86-64. The joint solver does not touch any
of the SIMD kernels and the baseline x86-64 target has no FMA for `-ffp-contract=fast`
to use, so on this machine those scenes are mostly measuring link time optimization
moving code around.

### Where the time goes

`sample` profiles of `convex_pile` on the M3 Ultra, top of stack, worker idle time
excluded. Box3D spends roughly 78% of its compute in the SAT edge query. fast3d still
has the same function on top, but it is down to roughly 48% of a run that is three
times shorter.

| Box3D (7.7 s/run) | samples | | fast3d (2.6 s/run) | samples |
| --- | ---: | --- | --- | ---: |
| b3QueryEdgeDirections | 11135 | | b3QueryEdgeDirections | 7239 |
| b3FindHullSupportVertex | 1487 | | b3DynamicTree_Query | 2584 |
| b3DynamicTree_Query | 991 | | b3QueryFaceDirections* | 2187 |
| b3QueryFaceDirections | 270 | | b3SolveContacts_Convex | 774 |
| b3UpdateConvexContact | 223 | | b3CollideHulls | 760 |

*LTO inlined the support vertex scan into the face query, so its time reports there.

The two profiles cover the same 5 second window, but a fast3d run is 3x shorter, so
equal sample counts mean fast3d is doing that work 3x faster. Per run, the edge query
costs about 4.5x less than in Box3D.

## What did it cost?

- Cross platform determinism: gone. Floating point contraction changes results across
  compilers and architectures.
- Box3D's own `DeterminismTest` fails on Apple Silicon, because FMA rounds differently
  in the scalar and SIMD solver paths, so results vary with worker count. This is
  expected: it is exactly the nondeterminism Box3D avoids by pinning contraction off.
- On x86-64 the full Box3D unit test suite passes, including determinism, because the
  baseline target has no FMA and the SIMD kernels preserve results exactly.

**The switch is the whole story, and it has been measured both ways.** On an M3 Ultra,
release build, `DeterminismTest` fails with `BOX3D_GO_FAST=ON` (the failing subtest is
`MultithreadingTest`, matching the worker-count explanation above) and the entire suite
passes with `BOX3D_GO_FAST=OFF`. Nothing else changed between the two runs. The option
reaches no source file — it only selects `-ffp-contract=fast` plus LTO, or
`-ffp-contract=off` — so contraction is the sole cause of the divergence here, not
thread scheduling or accumulation order.

A red `DeterminismTest` in a default build is therefore the test doing its job, not a
bug to be skipped. If both promises matter to you, run the suite in both
configurations.

## What didn't work

This fork exists to find out what Claude Code can do to a mature engine, so the
measured misses belong here too. They cost a day each to rule out, and recording them
is cheaper than someone repeating them.

- **`restrict` / `__restrict__` on the hot paths.** The classic advice: C and C++ must
  assume two pointers may alias, which blocks keeping values in registers across
  stores and blocks auto-vectorization, and one keyword removes the obstacle. It buys
  nothing here. Annotating the contact solver's body gather and scatter, the polygon
  clipper, the SoA support-vertex helpers and the velocity integration changed **zero
  of 304 functions** in the fast3d benchmark binary — the machine code is identical
  before and after, so the timings could not differ and did not. `-Rpass-analysis`
  reports no aliasing-blocked loops in either build.

  The reason is structural rather than marginal, and it is why this is unlikely to
  change: the hot loops are already hand-written NEON and SSE intrinsics over SoA
  data, so auto-vectorization was never on the table; the helpers taking pointer pairs
  are `static inline` and get inlined into callers that hold the buffers as locals, so
  the function boundary the optimization depends on does not survive to codegen; ThinLTO
  already gives cross-translation-unit visibility; and Clang's type-based alias analysis
  already separates `b3BodyState` from `b3BodySim`. `restrict` pays where a compiler is
  blind at a function boundary. This codebase has systematically removed those
  boundaries.

## Should I use this?

No. **DO NOT USE THIS LIBRARY.** Use [Box3D](https://github.com/erincatto/box3d).
Any optimization here that holds up will be ported back into vanilla Box3D, where it
will be maintained, tested, and correct across platforms. This fork will not be.

## Building

Same as [Box3D](https://github.com/erincatto/box3d#building-all-platforms). Add
`-DBOX3D_GO_FAST=OFF` if you want your determinism back, at which point you have
built Box3D with extra steps.

## License

MIT, same as Box3D. All the hard parts are Erin Catto's.
