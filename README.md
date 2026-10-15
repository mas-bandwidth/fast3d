# fast3d

**Box3D, but faster.**

> ## ⚠️ DO NOT USE THIS LIBRARY
>
> This repository exists for exactly one reason: to answer the question, can we
> throw Claude Code at Box3D and make it faster? The answer is yes. But
> [Erin Catto](https://github.com/erincatto) is much smarter than us, and any good
> optimizations found here may be offered to vanilla
> [Box3D](https://github.com/erincatto/box3d) — so you should just use that.

## What is this?

fast3d is a fork of [Box3D](https://github.com/erincatto/box3d) used as a test bed
for one experiment: point [Claude Code](https://claude.com/claude-code) at a mature,
carefully optimized physics engine and see whether it can find real speedups.

It can. The first round leaned on floating point contraction (FMA), which gave up the
cross platform determinism Box3D guarantees. That trade was reversed on 2026-10-15: fast3d
builds with contraction off, as Box3D does. Its determinism tests give the same results
on Linux x86-64 (gcc 15, clang 21) and macOS arm64 (Apple clang), in Debug and optimized
builds. Box3D has since made the first round's other source changes itself, so how much
faster fast3d is than Box3D today has not been re-measured.

## What changed?

1. **`-ffp-contract=fast`** *(removed)* — Box3D pins floating point contraction off for
   [cross platform determinism](https://box2d.org/posts/2024/08/determinism/).
   fast3d traded that determinism for fused multiply-add behind a `BOX3D_GO_FAST` CMake
   option. The option is gone, and contraction is pinned off again.
2. **Link time optimization** *(removed)* — it was part of the same option.
3. **SIMD Gauss map edge rejection** *(retired)* — fast3d's `b3QueryEdgeDirections`
   tested four hull edge pairs per iteration (NEON and SSE2), and was most of the win
   on hull-heavy scenes. Box3D has since shipped its own
   [SIMD hull collision](https://github.com/erincatto/box3d/pull/93) that vectorizes
   the whole pipeline over SoA hull data. That work is ported here and supersedes the
   fast3d kernel — the benchmark tables below predate it on both sides.
4. **Transposed body gathers** *(Box3D now does this too)* — the contact solver gathers
   body state with vector loads and 4x4 transposes instead of building lanes one float at
   a time. Box3D's 79fc559 does the same, and fast3d now uses Box3D's version.
5. **NEON support vertex scan** *(Box3D now does this too)* — four vertices per iteration
   with `vld3q` deinterleave. Box3D's adec25b vectorizes the scan on every SIMD target with
   a separate multiply and add, and fast3d now uses Box3D's version.
6. **`b3BodyState` padded to 64 bytes** *(Box3D now does this too)* — Box3D's 79fc559 lays
   it out in 64 bytes as well.

## Benchmarks

These tables were measured on the first-round build, with FMA contraction and link time
optimization on. That build no longer exists, so they are a record of the experiment, not
a description of fast3d today.

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
to use, so on this machine those scenes were mostly measuring link time optimization
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

In the first round, cross platform determinism. FMA rounds differently than a separate
multiply and add, so an optimized Apple Silicon build failed Box3D's own `DeterminismTest`
while x86-64, whose baseline target has no FMA, still passed. That cost was too high, so
the trade was reversed: with contraction off, every build measured passes the determinism
tests on both architectures.

## Should I use this?

No. **DO NOT USE THIS LIBRARY.** Use [Box3D](https://github.com/erincatto/box3d).
Any optimization here that holds up may be offered to vanilla Box3D, where it would be
maintained, tested, and correct across platforms.

## Building

Same as [Box3D](https://github.com/erincatto/box3d#building-all-platforms).

## License

MIT, same as Box3D. All the hard parts are Erin Catto's.
