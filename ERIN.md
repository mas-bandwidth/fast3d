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

---

## Hypothesis backlog
*(prioritized; one per night; tagged det-preserving vs det-breaking)*

1. **[det-preserving] Software prefetch in the contact-solver body-state gather.**
   `__builtin_prefetch` the next iteration's `stateA/stateB` (or the next gather batch's
   indices) during the current constraint solve, to hide the scattered-load latency in
   `b3GatherBodies` / the solve/relax loops in `contact_solver.c`. Prefetch changes no float
   results → determinism-safe. *Predict:* small but real, biggest on contact-heavy
   convex_pile / junkyard; likely larger on the memory-latency-bound single-thread EPYC than
   on the 8-worker Mac. Target >2% on EPYC convex_pile. **← Night 1.**

2. **[det-breaking, EPYC-only] Build the EPYC with AVX2 + FMA (`-march=znver4` or
   `-mavx2 -mfma`).** The current x86 build is SSE2 4-wide with no hardware FMA. Just letting
   the compiler use AVX2 + FMA (no source change) could be a large single-thread win on the
   contact-heavy scenes. Determinism-breaking (FMA + wider reductions), so fork-only — but
   potentially the biggest EPYC number available. Measure carefully; classify strictly.

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
