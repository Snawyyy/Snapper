# Snapper

Read `ARCHITECTURE.md` before changing code: it says which module and
manager a change belongs in.

## Build and test

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

`ctest` also runs the Snawy's Law linter and the layer check, so a
green run means the code builds, passes its tests, follows the rules
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
- Keep `ARCHITECTURE.md` true: a new manager or module goes in it in the
  same commit.

## Where we stopped

Update this section at the end of every session: the last thing done,
then the next steps. Full story: `docs/session-2026-10-08.md`.

Last done (2026-10-09, later): the owner loved the prototype and kept
all of it. Redesign steps 1 to 7 of section 12 in the design doc are
built and merged into `ui-test` (not pushed, not on main): Motion table
and View > Motion switch, scrub labels and math in number fields, hover
and big grab zones, toasts with Undo and the refusal shake, drag chip
and ghost, key pop/puff/rewind/slide, and the top bar. Owner answers
recorded in the design doc: Teto red picks, sounds on and quiet,
tiles animate on hover, key chip bar always over picked keys. Owner
test fixes merged: typed math now applies, every greyed control says
why, drag and wheel chips sit clear of the cursor, ghost is a faint
copy of the dragged thing. Lint is down to 121 old breaks.

Next steps:

1. The owner tests the latest `ui-test` build; fix what they find, one
   change at a time, a test per bug.
2. Performance is bad in the app: profile a Release build first (the
   tested build is Debug), then fix the slow paths.
3. Build redesign steps 8 to 13 in order: key chip bar, inspector
   reshape, stage zoom/pan/guides, cards and drag-in, rig by touch,
   extras (sounds go in here, on and quiet by default).
4. Fix the 121 old lint breaks so the `lint` ctest goes green.
5. Merge to main only when the owner says "merge".

Agents: run test binaries only with `QT_QPA_PLATFORM=offscreen`, never
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
