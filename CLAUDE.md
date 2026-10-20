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

## Rules for every change

- Put logic in the manager that owns the concern. A widget never decides
  anything a test should check.
- Change the project only through a manager and `HistoryManager`.
- Reuse the shared pieces: one key type, one sampler, one renderer, one
  edit path. If something looks like a copy of existing code, it is.
- A small tweak to existing tested logic (a number, a label) needs no
  new test, just a rebuild.
- Keep `ARCHITECTURE.md` and `docs/feature-map.md` true: a new manager,
  module, feature area or kind site goes in them in the same commit.

## Where we stopped

Update this section at the end of every session: the last thing done,
then the next steps. Older history: `git log` and
`docs/session-2026-10-08.md`.

Last done (2026-10-20, branch `ccr-042c021f-aywj12`, cut from
`tape-keys`): the PNG dolls are gone, replaced by Moho style
characters (`docs/redesign/moho-plan.md`, ARCHITECTURE.md
"Characters"). On the owner's word ("we scrap old") there is no
converter: an old project opens with its doll layers left out and a
note. Gone with the dolls: the Krita exporter and menu, the doll
library and its folders, the Rig tab, saved poses, poking, dot groups,
smart dials, skins, objects and holds, warp dots, jelly and cloth,
joint snap, smooth warp, lean and swivel, drawing on a piece. What is
left works on whole layers: picks, links, keys and Comp targets name a
layer; the Pose stage moves, scales and turns layers and drags a
picked character's bones; the Cast panel has Characters (New, Edit,
Put on stage, drag to the stage) and Layers; draw mode draws
storyboard frames only. Fixed on the way: the project file lost every
character on reading. Every ported test passes, and the old failures
(refusal reasons, scrub, video page, window) are fixed. The app was
not launched (owner's rule).

Next steps:

1. The owner tries characters by eye: the Character tab (draw, bones,
   bind, switches, smart bones), posing on the Pose tab, the Cast.
2. Then the plan's next phases (`docs/redesign/moho-plan.md`): what
   it lists as not built yet.
3. Merge to main only when the owner says "merge".

Build here (cloud): micromamba in `/opt/mm`; Qt 6.12 and ffmpeg 9 in
`/opt/qtenv`; ccache and mold in `/opt/tools/bin` (put it on PATH).
Configure with `PKG_CONFIG_PATH=/opt/qtenv/lib/pkgconfig cmake --preset
debug -DCMAKE_PREFIX_PATH=/opt/qtenv
-DSNAPPER_CCACHE=/opt/tools/bin/ccache -DCMAKE_LINKER_TYPE=MOLD`. Only
4 cores: a full build takes about 30 minutes, so build the module
(`snapper_edit`, `snapper_ui`...) or the one test target you touch. If
a new Q_OBJECT header links with "undefined vtable", delete that
module's `*_autogen` folder in the build dir and build again.

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
- Selected things get a Teto red outline; draggable handles are yellow;
  what is on or focused wears red too (picked tab, toggled button, check
  marks, focused field ring, title plate strip).
- Handles look small and grab big.
- Dials turn with the mouse wheel and sliders step with it; a double
  click resets either one (a press alone changes nothing).
- Right click changes a node's mode; double click on empty space adds
  one; Delete removes the picked ones; Shift adds to the pick.
- The pick rule, everywhere (`ui/pick_keys.h`): Ctrl flips one in or
  out and wins over Shift; Shift adds; a plain click on something picked
  keeps the pick for a drag; a box adds with Shift and removes with Ctrl.
- Panels follow the Pose box: raised title plates over sunken frames,
  settings indented under their title, golden ratio splits, square
  corners, built from `ui/panel_kit.h`.
