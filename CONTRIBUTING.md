# Contributing to fast3d

Pull requests are welcome. If a change is good and useful, we will merge it.

fast3d is a fork of [Box3D](https://github.com/erincatto/box3d) by Erin Catto, and Box3D
does not take pull requests. That is Erin's call for his project, and a reasonable one. It
is not our policy here.

## Read this first

fast3d is a test bed, not a library to ship. It exists to find out whether pointing
[Claude Code](https://claude.com/claude-code) at a mature, carefully optimized physics
engine turns up real speedups, and the answer so far is yes. Optimizations that hold up
may be offered to Box3D, where they would be maintained and correct across platforms.
The [README](README.md) says do not use this library, and means it.

So an optimization you find here is worth more upstream than it is here. If your change is
a genuine, portable, determinism-preserving win, consider raising it with
[erincatto/box3d](https://github.com/erincatto/box3d/issues) as well. Bugs in Box3D
itself, unrelated to the fast3d experiment, belong upstream there too.

Issues are turned off on this repository, deliberately: it is an experiment, not a
supported library. A pull request is the way in, and its description and comments are the
place to discuss it.

## What a good contribution looks like here

Speed, measured, with determinism kept. fast3d must produce bit-identical results across
platforms and compilers on its own determinism scenes (`test/test_determinism.c`), the same
guarantee Box3D makes. fast3d may drift from Box3D's exact numbers for speed, but never
from itself: a change that moves the pinned values has to move them identically on x86-64
and arm64, and the pull request updates the pinned step and hash values in
`test/test_determinism.c` with both measurements.

- Bring numbers. Say what you measured, on what hardware, with what compiler and build
  type, and how many runs. `benchmark -t=4 -w=4 -r=2` is the shape the README uses. Note
  that the benchmark CLI takes equals-form flags only (`-w=4`, not `-w 4`); a
  space-separated flag is silently ignored, which quietly changes what you measured.
- The full test suite has to pass, in Debug and in an optimized build.
- No FMA contraction, reassociation or fast-math. The build pins `-ffp-contract=off`, and a
  change that needs it lifted is out of scope.

## Opening a pull request

1. Fork the repository and branch from `main`.
2. Keep the pull request to one logical change. If you find yourself touching unrelated
   things, split them into separate pull requests.
3. Say in the description what the change does and why.
4. Build and test locally before you send it:

   ```bash
   ./build.sh
   ./build/bin/test
   ```

   Then make sure CI is green on the pull request.
5. Match the surrounding code. Library sources are C17, samples are C++20. The repository
   ships a `.clang-format`; use it on code you add or edit. Please do not mass-reformat
   files you are not otherwise changing, because it buries the real change.

## License

fast3d is MIT, the same as Box3D, and it stays MIT. Contributions are accepted under
that license. There is no contributor agreement to sign.

## Credit

Contributors keep their name in the commit history, and a change worth calling out gets
called out. If your work gets ported upstream to Box3D, it goes with your name on it.

## Questions

Ask in the pull request. For anything about Box3D itself, upstream at
[erincatto/box3d](https://github.com/erincatto/box3d/issues) is the better room.
