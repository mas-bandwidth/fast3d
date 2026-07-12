# fast3d

**Box3D, but faster.**

> ## ⚠️ DO NOT USE THIS LIBRARY
>
> This repo is a joke (love you Erin). Just like Fixed3D, it exists to make
> [Erin Catto](https://github.com/erincatto) mad. If you want a 3D physics engine
> for your game, use the real thing: [Box3D](https://github.com/erincatto/box3d).
>
> Seriously. **DO NOT USE THIS LIBRARY.**

## What is this?

fast3d is a fork of [Box3D](https://github.com/erincatto/box3d) that asks the question:
what happens if you take a physics engine written by the guy who invented modern game
physics engines and simply refuse to care about one of the things he cares about?

It turns out it goes faster.

## What did you do?

1. **`-ffp-contract=fast`** — Box3D pins floating point contraction off for
   [cross platform determinism](https://box2d.org/posts/2024/08/determinism/).
   fast3d does not have cross platform determinism. fast3d has fused multiply-add.
   Controlled by the `BOX3D_GO_FAST` CMake option, default `ON`, obviously.
2. **Link time optimization** — on by default in release builds.
3. **SIMD Gauss map edge rejection** — `b3QueryEdgeDirections` tests four hull edge
   pairs per iteration (NEON and SSE2). The Gauss map arc test rejects nearly every
   edge pair, so the wide reject path is nearly the whole loop. This is most of the
   win on hull-heavy scenes.
4. **Transposed body gathers** — the contact solver gathers body state with vector
   loads and 4x4 transposes instead of building lanes one float at a time.
5. **NEON support vertex scan** — four vertices per iteration with `vld3q` deinterleave.
6. **`b3BodyState` padded to 64 bytes** — there was a `todo_erin` on it. You're welcome.

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
moving code around. You get a 2.5x convex pile, Erin gets 2% of a joint grid. Fair trade.

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
  in the scalar and SIMD solver paths, so results vary with worker count. Working as
  intended. This is the part that makes Erin mad.
- On x86-64 the full Box3D unit test suite passes, including determinism, because the
  baseline target has no FMA and the SIMD kernels preserve results exactly.

## Should I use this?

No. **DO NOT USE THIS LIBRARY.** Use [Box3D](https://github.com/erincatto/box3d).
Erin will fold anything actually good from here into the real engine in a weekend,
probably while writing a blog post explaining why my version of it is subtly wrong.

## Building

Same as [Box3D](https://github.com/erincatto/box3d#building-all-platforms). Add
`-DBOX3D_GO_FAST=OFF` if you want your determinism back, at which point you have
built Box3D with extra steps.

## License

MIT, same as Box3D. All the hard parts are Erin Catto's. The determinism-flavored
vandalism is mine.
