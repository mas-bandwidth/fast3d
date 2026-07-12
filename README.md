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

The benchmark suite that ships with Box3D: 8 workers, minimum of 4 runs, stock Box3D
built with its default release settings, fast3d built with its default release settings.
Benchmarks interleaved per scene so neither side gets the warm half of the machine.

### Apple M3 Ultra (NEON), 8 workers

| benchmark | Box3D (ms) | fast3d (ms) | speedup |
| --- | ---: | ---: | ---: |
| convex_pile | 8976 | 3181 | **2.82x** |
| junkyard | 3590 | 2944 | 1.22x |
| many_pyramids | 325 | 277 | 1.17x |
| washer | 5720 | 5049 | 1.13x |
| large_pyramid | 368 | 326 | 1.13x |
| joint_grid | 166 | 153 | 1.08x |
| trees100 | 97 | 90 | 1.07x |
| trees50 | 113 | 105 | 1.07x |
| trees25 | 191 | 180 | 1.06x |
| rain | 440 | 423 | 1.04x |
| large_world | 19 | 20 | wash* |

*large_world runs in ~20 ms and flips winner run to run.

### AMD EPYC 9124 (SSE2), 1 worker

| benchmark | Box3D (ms) | fast3d (ms) | speedup |
| --- | ---: | ---: | ---: |
| convex_pile | 63232 | 27678 | **2.28x** |
| junkyard | 32529 | 29162 | 1.12x |
| large_pyramid | 3510 | 3342 | 1.05x |
| washer | 42754 | 41230 | 1.04x |
| many_pyramids | 4150 | 4027 | 1.03x |
| rain | 2992 | 2998 | 1.00x |
| joint_grid | 1845 | 1891 | 0.98x |
| trees100 | 276 | 304 | 0.91x* |
| trees50 | 436 | 445 | 0.98x |
| trees25 | 1019 | 1060 | 0.96x |
| large_world | 13 | 11 | wash |

*Re-measured three times interleaved: 269 vs 273 ms, about 1.5% slower. The mesh-heavy
trees scenes lose a couple percent on x86-64 and I will be taking no further questions.

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
