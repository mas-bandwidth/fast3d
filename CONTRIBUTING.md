# Contributing to fast3d

Pull requests are welcome. If a change is good and useful, we will merge it.

fast3d is a fork of [Box3D](https://github.com/erincatto/box3d) by Erin Catto, and Box3D
does not take pull requests. That is Erin's call for his project, and a reasonable one. It
is not our policy here.

## Read this first

fast3d is a test bed, not a library to ship. It exists to find out whether pointing
[Claude Code](https://claude.com/claude-code) at a mature, carefully optimized physics
engine turns up real speedups, and the answer so far is yes. Optimizations that hold up
get ported back into Box3D, where they will be maintained and correct across platforms.
The [README](README.md) says do not use this library, and means it.

So an optimization you find here is worth more upstream than it is here. If your change is
a genuine, portable, determinism-preserving win, consider raising it with
[erincatto/box3d](https://github.com/erincatto/box3d/issues) as well. Bugs in Box3D
itself, unrelated to the fast3d experiment, belong upstream there too.

Issues are turned off on this repository, deliberately: it is an experiment, not a
supported library. A pull request is the way in, and its description and comments are the
place to discuss it.

## What a good contribution looks like here

Speed, measured. This fork trades cross-platform determinism for performance
(`BOX3D_GO_FAST`, on by default), so the usual determinism constraint does not bind, but
correctness does.

- Bring numbers. Say what you measured, on what hardware, with what compiler and build
  type, and how many runs. `benchmark -t=4 -w=4 -r=2` is the shape the README uses. Note
  that the benchmark CLI takes equals-form flags only (`-w=4`, not `-w 4`); a
  space-separated flag is silently ignored, which quietly changes what you measured.
- The full test suite has to pass. A speedup that changes results is a bug report, not an
  optimization.
- Say whether the win survives with `-DBOX3D_GO_FAST=OFF`. A win that only exists with
  GO_FAST on is a different, smaller claim.

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

## Contributor Assignment Agreement

Before your first contribution can be merged you need to sign the
[Contributor Assignment Agreement](https://github.com/mas-bandwidth/.github/blob/main/CAA.md).
A bot posts the link on your first pull request. Signing is one comment, it takes a
minute, and it covers everything you contribute to any Más Bandwidth repository after
that.

Read it before you sign. It is a copyright assignment, a stronger grant than the license a
CLA usually asks for: you assign copyright in your contribution to Más Bandwidth LLC, and
you get back a perpetual license to use your own work for any purpose. We ask for it so
these libraries can be relicensed in future without tracking down every past contributor
for permission. If your contribution includes third-party material, identify it when you
submit; that material stays under its own license.

fast3d is MIT, the same as Box3D, and the Box3D-derived material stays MIT.

## Credit

Contributors keep their name in the commit history, and a change worth calling out gets
called out. If your work gets ported upstream to Box3D, it goes with your name on it.

## Questions

Ask in the pull request. For anything about Box3D itself, upstream at
[erincatto/box3d](https://github.com/erincatto/box3d/issues) is the better room.
