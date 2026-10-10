# Snapper

Start with `docs/feature-map.md`: for each feature area it names the
`ARCHITECTURE.md` section to read, the files, the manager that owns
the edits and the test file, and for each kind enum every file a new
kind touches. Read only that section of `ARCHITECTURE.md`, not all of
it.

## Build and test

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

The build folder lives on the fast drive (`~/.cache/snapper/<checkout
folder>/build`); `build` in the checkout links to it.

While working, build and run only the tests you touch (each
`tests/<module>/<name>.cpp` is its own target): one target rebuilds
in about a second, the whole build in about 20 s because every change
relinks every test program.

```sh
cmake --build --preset debug --target <name>
ctest --preset debug -R '^<name>$'
```

Edit with the Edit and Write tools, not shell scripts: the Snawy's
Law hook lints each file as it is saved, so breaks show up at once.
Before calling it done, one full build and `ctest --preset debug`.
`ctest` also runs the linter over every file and the layer check, so
a green run means the code builds, passes its tests, follows the rules
and respects the module order.

## Krita exporter

`tools/krita` is the Krita plugin. Install by linking
`snapper_doll_export.desktop` and the `snapper_doll_export` folder into
`~/.local/share/krita/pykrita`, then enable it in Krita's Python Plugin
Manager. It writes dolls to `~/.local/share/Snapper/Snapper/dolls`.

## Rules for every change

- Put logic in the manager that owns the concern. A widget never decides
  anything a test should check.
- Change the project only through a manager and `HistoryManager`.
- Reuse the shared pieces: one key type, one sampler, one renderer, one
  edit path. If something looks like a copy of existing code, it is.
- New logic leaves a test behind in `tests/<module>/`.
- A small tweak to existing tested logic (a number, a label) needs no
  new test, just a rebuild.
- Keep `ARCHITECTURE.md` and `docs/feature-map.md` true: a new manager,
  module, feature area or kind site goes in them in the same commit.

## Where we stopped

Update this section at the end of every session: the last thing done,
then the next steps. Full story: `docs/session-2026-10-08.md`.

Last done (2026-10-10, branch `workflow`, merged into `ui-test`):
agent speed. Past agent logs showed builds took most of their tool
time, so builds moved off the NTFS drive (a one-file change rebuilt in
70 s, now 20 s; one test target in under a second), ccache is on, mold
is used when installed, agent worktrees start from the current branch,
and `docs/feature-map.md` sends an agent straight to the section,
files, manager and test it needs. 22 old merged worktrees removed.

Before that (2026-10-10): warp mesh detail lowered (Few 10, Normal 25,
Many 60); reach set on many picked dots at once (wheel and a Reach
field) and copied by Mirror; drag nodes got a kind (Drag, Cloth,
Jelly) replacing the Physics tick; Jelly physics (one smooth blob
that keeps its area, bulging where it is not squeezed); feels and
presets (firm/heavy pad, Pudding, Silk, Snappy...); the dot panel
rebuilt as material tiles, preset chips, feel pad with a live wobble
and a Fine tune fold; stage dots drawn by kind; hold P and drag to
poke a doll and feel its physics without keys. All on branch `jelly`,
merged into `ui-test` (not pushed, not on main). Poke moves only the
dots, not the body (owner to say if the body should shove too).

Next steps:

1. The owner tests the latest `ui-test` build (jelly, dot panel, P
   poke); fix what they find, one change at a time, a test per bug.
2. Performance is bad in the app: profile a Release build first (the
   tested build is Debug), then fix the slow paths.
3. Build redesign steps 8 to 13 in order: key chip bar, inspector
   reshape, stage zoom/pan/guides, cards and drag-in, rig by touch,
   extras (sounds go in here, on and quiet by default).
4. Faster links: mold is in; next, think about one test program per module instead of one per file, since
   every change relinks about 100 of them.
5. Merge to main only when the owner says "merge".

Agents: hand code changes to the `snapper-dev` agent (six tools, so
it starts at about 14k tokens against 44k for general-purpose; it has
no web, browser or sub-agents, so research goes to another agent).
Run test binaries only with `QT_QPA_PLATFORM=offscreen`, never
launch `build/snapper`; merge `ui-test` into a branch before handing it
back so conflicts are solved where the code was written.

## UX language

- Keys hold (step) unless told to ease.
- Every drag is one undo step; Escape cancels it.
- A control that cannot act is greyed out with a tooltip saying why.
- Undo and redo name what they undo ("Undo Rotate head").
- New things appear where they cause no jump.
- Selected things get a Teto red outline; draggable handles are yellow.
- Handles look small and grab big.
- Right click changes a node's mode; double click on empty space adds
  one; Delete removes the picked ones; Shift adds to the pick.
