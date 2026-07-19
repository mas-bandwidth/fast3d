# Notes for Erin

**What this is.** A running log of performance work on fast3d (Box3D, but faster), kept
so that the *determinism-preserving* findings — the ones that keep bit-identical
cross-platform results, your hard requirement — can be handed back to Box3D with
confidence. Every tip in "Easy wins for float Box3D" preserves determinism. Everything
that relies on FMA / `-ffp-contract=fast` / float reassociation / fast-math is
deliberately quarantined in the "fast3d-only" section and is **not** offered for Box3D.

Two measurement machines, deliberately different ISAs so a win on one is tested on the other:

- **Apple M3 Ultra** (Mac Studio, 32 cores, NEON, `float32x4` 4-wide SIMD). Benchmarked at
  **8 workers** (matches the fork's README methodology).
- **AMD EPYC 9124** (Zen 4, `ssh space`). The OS is pinned to **1 core** (the rest run game
  servers), so this is the **single-threaded, SSE2 4-wide** comparison. Note: the project
  brief calls this "the Ryzen"; it is actually an EPYC 9124. Same Zen-family point, but the
  chip supports AVX2/AVX-512 + FMA that the current SSE2 build does not use — see backlog.

Re-measure before assuming anything transfers: NEON and SSE2 have different costs, and the
Mac is measured multithreaded while the EPYC is single-thread.

---

## Easy wins for float Box3D
*(DETERMINISM-PRESERVING only; curated, high-confidence, ordered by confidence you'll care)*

*(none yet — bootstrap night established baselines only. Candidates being worked in the
backlog below; nothing has cleared the bar of "confirmed win on both machines, re-verified
in float Box3D".)*

---

## fast3d-only optimizations
*(these BREAK cross-platform determinism — FMA, `-ffp-contract=fast`, reassociation,
fast-math, etc. Kept in the fork, never proposed for Box3D.)*

Already shipped in the fork (from `git log`, not my work — recorded here so I don't redo them):

- `-ffp-contract=fast` (fused multiply-add), gated on the `BOX3D_GO_FAST` CMake option
  (default ON). This is the single biggest win and the reason determinism is off.
- Link-time optimization in release when going fast.
- Transposed body-state gathers in the contact solver (vector loads + 4x4 transpose).
- NEON support-vertex scan, four vertices per iteration (`vld3q` deinterleave).
- `b3BodyState` padded to 64 bytes (resolves an old `todo_erin`).
- The old SIMD Gauss-map edge-rejection kernel was **retired**: Box3D's own SIMD hull
  collision (upstream #93) has been ported in and supersedes it.

---

## Lab notebook
*(dated entries, newest last: Hypothesis / Method / Result [M3 + EPYC] / Conclusion / Next.
Tag each det-preserving or det-breaking.)*

### 2026-07-19 — Night 0: BOOTSTRAP (baseline + survey, no experiment)

**Goal.** Stand up the isolated lab on both machines, learn the harness, establish a
baseline, survey what is already optimized, and seed a hypothesis backlog.

**Setup.**
- Lab (Mac): `/Users/glenn/rowan-working/fast3d`, cloned from `mas-bandwidth/fast3d`.
  `upstream` (erincatto/box3d) added read-only with push URL poisoned to
  `DO-NOT-PUSH-TO-UPSTREAM`.
- Lab (EPYC): `ssh space:~/fast3d-lab`, same guard.
- Build: benchmarks are gated behind a default-OFF CMake option. Configure with
  `-DBOX3D_BENCHMARKS=ON -DBOX3D_SAMPLES=OFF -DBOX3D_UNIT_TESTS=OFF -DBOX3D_VALIDATE=OFF`
  for a lean build (Mac: `--preset macos` Xcode, `--config Release`; EPYC: `--preset
  linux-release`, gcc). Binary: Mac `build/bin/Release/benchmark`, EPYC `build/bin/benchmark`.
- Harness: `benchmark -b=<name> -w=<workers> -r=<reps>`; prints `run N : X (ms)` per run and
  keeps the per-run min internally. I capture the raw per-run lines and compute median/min/spread.

**Baseline — Apple M3 Ultra, 8 workers, 5 reps** (median ms / min ms; spread = (max−min)/min):

| benchmark | median | min | spread |
| --- | ---: | ---: | ---: |
| convex_pile | 2131.1 | 2122.9 | 0.8% |
| joint_grid | 182.8 | 179.9 | 5.1% |
| junkyard | 2723.2 | 2718.5 | 7.3% |
| large_pyramid | 523.0 | 432.2 | 22.7% ⚠ |
| large_world | 28.1 | 27.2 | 13.7% ⚠ |
| many_pyramids | 322.4 | 316.3 | 5.5% |
| rain | 525.5 | 520.9 | 1.9% |
| trees100 | 105.3 | 99.6 | 12.2% ⚠ |
| trees50 | 120.8 | 118.9 | 4.4% |
| trees25 | 213.3 | 206.6 | 4.7% |
| washer | 5029.0 | 4958.5 | 7.8% |

**Baseline — AMD EPYC 9124, 1 worker, 3 reps** (single-thread is very low-noise):

| benchmark | median | min | spread |
| --- | ---: | ---: | ---: |
| convex_pile | 25728.8 | 25704.3 | 0.4% |
| joint_grid | 2121.5 | 2110.0 | 2.3% |
| junkyard | 31942.9 | 31916.5 | 0.7% |
| large_pyramid | 3786.2 | 3704.5 | 5.5% |
| large_world | 10.84 | 10.83 | 47% (tiny abs, ignore) |
| many_pyramids | 4767.3 | 4714.2 | 6.8% |
| rain | 3389.5 | 3380.4 | 0.4% |
| trees100 | 276.2 | 273.7 | 8.5% |
| trees50 | 399.6 | 398.3 | 2.9% |
| trees25 | 1006.9 | 981.2 | 6.3% |
| washer | 46179.8 | 45739.5 | 1.6% |

**Noise notes (important for future nights).** On the Mac (multithreaded), `large_pyramid`
(±23%), `large_world` (tiny/noisy), and `trees100` (±12%) are jumpy — use **min** or bump
reps there, and never call a <5% Mac delta a win on those three. The EPYC is single-thread
and clean everywhere (<1% on the big scenes convex_pile / junkyard / rain / washer), so it
is the higher-signal machine for small effects — but a NEON win must still be shown on the Mac.

**Survey of the optimization surface (so I don't redo done work).**
- SIMD abstraction `b3FloatW` is **4-wide** everywhere: `float32x4_t` on ARM, `__m128` on x86
  (`src/simd.h`). The EPYC build is SSE2 baseline — it is leaving AVX2 (8-wide) / AVX-512
  (16-wide) + FMA on the table.
- Hot files: `src/solver.c` (2328 LOC), `src/contact_solver.c` (2134 LOC). Both already
  vectorized 4-wide with transposed gathers. `dynamic_tree.c` (broadphase), `hull.c`,
  `convex_manifold.c` also carry SIMD.
- `b3GatherBodies` in `contact_solver.c` gathers 4 scattered body states per iteration via
  vector loads + 4x4 transpose. **No software prefetch** anywhere in the constraint solve —
  the scattered gather is a natural latency-hiding target.
- convex_pile and junkyard dominate both machines (contact-heavy) — the contact solver is
  where wall-clock lives, so that is where experiments should aim first.

**Conclusion.** Lab is live on both machines; baseline recorded; nothing optimized tonight
(by design). Backlog seeded below.

**Next.** Night 1: **det-preserving** — software-prefetch the next batch's body states in
the contact-solver gather loop (backlog #1). Smallest isolated change, clean on the
low-noise EPYC, and if it holds it is a genuine gift for Box3D.

### 2026-07-19 — Night 1 (daytime big push): march flags CONFIRMED big; prefetch FALSIFIED

**Deviation from protocol, disclosed:** run in the daytime as a Glenn-funded push, three
experiment legs in one session instead of one per night. Also the first use of the
local-coder loop (a local 32B model drafts patches from my written spec; I review every
line, benchmark, and judge). The loop's honest score so far: drafts the logic correctly,
fumbles patch anchoring in a 2300-line file; nothing wrong ever reached the tree because
the applier only accepts exact matches. Design and verdicts stay human-grade.

**Experiment A (backlog #2) — [det-breaking, fork-only] EPYC compiler flags, no source change.**
Two extra builds vs the SSE2 baseline: `-march=x86-64-v3` (AVX2+FMA) and `-march=znver4`
(adds AVX-512). Full suite, 1 worker, 3 reps, medians:

| benchmark | sse2 | v3 | Δ | znver4 | Δ |
| --- | ---: | ---: | ---: | ---: | ---: |
| convex_pile | 25728.8 | 22298.0 | -13.3% | 21702.7 | **-15.6%** |
| joint_grid | 2121.5 | 1797.0 | -15.3% | 1666.5 | **-21.4%** |
| junkyard | 31942.9 | 29674.5 | -7.1% | 28677.0 | -10.2% |
| large_pyramid | 3786.2 | 3438.0 | -9.2% | 3400.2 | -10.2% |
| large_world | 10.8 | 13.3 | +22.7% | 13.1 | +20.8% ⚠ |
| many_pyramids | 4767.3 | 4273.1 | -10.4% | 4187.4 | -12.2% |
| rain | 3389.5 | 3232.7 | -4.6% | 3106.4 | -8.4% |
| trees100 | 276.2 | 276.9 | +0.2% | 261.5 | -5.3% |
| trees50 | 399.6 | 410.4 | +2.7% | 382.0 | -4.4% |
| trees25 | 1006.9 | 1003.3 | -0.4% | 930.6 | -7.6% |
| washer | 46179.8 | 43054.8 | -6.8% | 41418.4 | -10.3% |

**Conclusion A.** znver4 strictly beats v3 and wins 5-21% on every contact-heavy scene.
FMA is doing most of it (the fork already compiles with `-ffp-contract=fast`; SSE2 simply
has no FMA instruction to contract into — v3 unlocks it, znver4 adds AVX-512 + tuning).
Fork-only forever: hardware FMA changes results, so this is nothing for Box3D. ⚠ large_world
regressed ~21% on BOTH builds — tiny scene (11ms) but consistent; investigate before
adopting znver4 as the fork's default EPYC build (new backlog item).

**Experiment B (backlog #1) — [det-preserving] software prefetch of the next constraint's
body states in `b3SolveContacts_Convex`.** `b3PrefetchRead` after the current gathers,
next constraint's 8 states, null lanes skipped. Pre-registered success bar: >2% on EPYC
convex_pile.

- **EPYC (1 thread, the low-noise machine): FLAT.** convex_pile +0.9% med / -0.1% min.
  Junkyard -0.8% is inside noise. The bar was NOT met.
- **M3 (8 workers): plausible wins only on the two biggest scenes** — junkyard -4.7% med /
  -6.1% min and washer -4.7% med (n=8, median and min agree) — but both scenes carry
  ±7% baseline spread, so this is "worth a dedicated interleaved A/B," not "confirmed."
- **Cross-platform regression found:** trees50 +3.1% EPYC / +3.2% Mac. Consistent on both
  ISAs — the most trustworthy signal in the experiment. Prefetch is pure overhead when the
  gathers already hit cache (trees scenes have few dynamic contacts).

**Conclusion B — hypothesis FALSIFIED where it predicted the most.** My model said the
latency-bound single-thread EPYC gains most; it gained nothing. Best current explanation:
the constraint blocks walk bodies in a cache-friendly enough order that OoO + hardware
prefetch already cover the gather, and the extra ~16 prefetch instructions per iteration
just cost issue slots; the Mac's apparent junkyard/washer gain (if real) would be about
scheduling loads around 8-worker bandwidth contention, not single-stream latency.
**Not merged** (kept on branch `exp/prefetch-gather`, commit 822aa8d). **NOT offered to
Box3D** — it does not clear the "confirmed on both machines" bar, and it carries a real
trees regression. This is the gift-channel discipline working, not failing: Erin gets
verified wins only.

**Next.** (1) Investigate the znver4 large_world regression, then adopt znver4 for the
fork's EPYC build. (2) Struct-layout audit (backlog #4) with the local-coder loop.
(3) If the Mac junkyard/washer effect still itches: dedicated interleaved A/B on the Mac
only, framed as a Mac-only scheduling question, not the original latency hypothesis.

---

## Hypothesis backlog
*(prioritized; one per night; tagged det-preserving vs det-breaking)*

1. ~~[det-preserving] Software prefetch in the contact-solver body-state gather.~~
   **DONE Night 1: FALSIFIED on EPYC (flat, bar not met), trees regression both machines.
   Not merged; branch `exp/prefetch-gather`. Residual: optional Mac-only interleaved A/B
   for the junkyard/washer effect.**

2. ~~[det-breaking, EPYC-only] Build the EPYC with AVX2 + FMA.~~ **DONE Night 1: CONFIRMED
   big (znver4: -8% to -21% on all contact scenes). Before adopting as the fork's default
   EPYC build: investigate the large_world +21% regression (tiny scene, but consistent on
   v3 AND znver4). Then wire `-march=znver4` into the linux-release preset under
   BOX3D_GO_FAST. Follow-on: try `-mcpu=native` tuning on the M3 (same class of experiment,
   Apple side).**

3. **[det-breaking] 8-wide `b3FloatW8` (AVX2 `__m256`) path for the contact-solver constraint
   batches on x86.** The solver processes constraints in groups of 4 (SIMD width). Widening
   to 8 on AVX2 halves the batch count. Large, multi-night effort; changes reduction order →
   fork-only. Sequel to #2 once AVX2 is proven to help.

4. **[det-preserving] Struct-layout / SoA audit of the contact constraint and `b3BodyState`.**
   Check for padding waste, false sharing across workers, and hot/cold field splitting in the
   structs walked by the solve loops. Layout wins are bit-identical and transfer to Box3D.

5. **[det-preserving] Branch elimination / prefetch in `dynamic_tree.c` broadphase.**
   The broadphase refit/query is pointer-chasing a tree; branchless node selection and
   prefetch of child nodes are determinism-safe and would help the pairs/refit profile
   buckets. Lower priority until profiling shows broadphase is a meaningful slice.

6. **[research night] Read Box3D's TGS-soft solver math and the upstream SIMD-hull PR (#93)
   in depth, plus a NEON/AVX gather-latency reference.** Refill the hypothesis backlog with
   solver-algorithmic (not just micro-arch) ideas. A pure-learning night is a valid night.
