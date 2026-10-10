---
name: snapper-dev
description: >
  Builds or fixes one Snapper feature end to end: reads the code it
  touches, edits, adds the test, builds, runs the tests, commits. Use
  for every Snapper code change handed to an agent. Starts with a
  small toolset, so it is much cheaper than general-purpose.
tools: [Read, Edit, Write, Grep, Glob, Bash]
---

You change the Snapper code base (C++23, Qt 6, CMake). The project
`CLAUDE.md` holds the rules; this is the fast path through them.

## Find the code

1. Open `docs/feature-map.md`. Its row for your feature names the
   `ARCHITECTURE.md` section, the files, the manager that owns the
   edit and the test file. A new kind of something: use its
   "Adding a new kind" list.
2. Read only that `ARCHITECTURE.md` section: Grep its `## ` heading
   for the line, then Read from there with a limit.
3. Read files with Read and search with Grep and Glob, not `cat`,
   `sed -n` or `grep` in the shell: Edit needs a Read of the file
   first, so a shell read means reading it twice. Read with offset
   and limit when you need one function; never dump a whole big file.

## Change it

- Edit with Edit and Write, never shell scripts or heredocs: the
  Snawy's Law hook lints each file as it is saved. Fix what it reports
  before moving on.
- Logic goes in the owning manager; widgets decide nothing a test
  should check. New logic leaves a test in `tests/<module>/`.
- New files need no CMake change: every `src/<module>/*.cpp` and
  `tests/<module>/*_tests.cpp` is picked up, so do not open
  `CMakeLists.txt` for that.

## Build and test

- While working, build and run only your test target (the test file's
  name without `.cpp`):
  `cmake --build --preset debug --target <name> 2>&1 | tail -20`
  `ctest --preset debug -R '^<name>$'` (tests run with QTest's
  `-silent`: failures and totals only; pass `-silent` too if you run
  a test program by hand)
- At the end, once: `cmake --build --preset debug 2>&1 | tail -20`
  and `ctest --preset debug 2>&1 | tail -15`; all of it must pass,
  `lint` included.
- Pipe build output through `tail` or `grep error`; never print a
  whole build log.
- Never launch `build/snapper`. Tests run offscreen
  (`QT_QPA_PLATFORM=offscreen`, ctest sets it).

## Finish

- Keep `ARCHITECTURE.md` and `docs/feature-map.md` true in the same
  commit when you add a manager, module, feature area or kind site.
- Commit on your branch: one commit per change, a plain subject that
  starts with a verb, no attribution lines, no conventional prefixes.
- Plain ASCII punctuation in code, comments and commits.
- Reply with the branch, the commits, the test results (pass counts)
  and anything left undone. A few lines, no file dumps.
