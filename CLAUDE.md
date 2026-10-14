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
- A small tweak to existing tested logic (a number, a label) needs no
  new test, just a rebuild.
- Keep `ARCHITECTURE.md` and `docs/feature-map.md` true: a new manager,
  module, feature area or kind site goes in them in the same commit.

## Where we stopped

Update this section at the end of every session: the last thing done,
then the next steps. Full story: `docs/session-2026-10-08.md`.

Also 2026-10-14, side branches cut from `joint-end-snap`, not pushed,
built, no tests, not seen by eye: `frame-size` (File > Frame size...
changes the frame after the project is made, content stays centered;
storyboard cels stretch and reel text follows the new shape, owner to
say if that should stay put) and `drawing-place` (a re-export that
grows a piece's box now carries its pivot, colliders, mesh and IK tip
along via `box_corner` in rig.json; the owner's "smaller drawing moves
down" was not reproduced, ask for the doll and a Krita screenshot if it
still happens). Merge them into one line of work when the owner says.

Last done (2026-10-14, branch `keyed-draw-order`, cut from
`joint-end-snap`, not pushed): draw order is keyed. `PoseField::kOrder`
in `PoseManager` (typed number, wheel steps, `StepOrder`; one undo step
named "Draw order head"); a limb's order adds to the rig's, a layer's
own order (a doll) draws it N places in front or behind of its place in
the shot's stack past any kind of layer (`LayersInDrawOrder`,
`anim/layer_order.h`; the renderer and `HitTest` both walk it); an Order
field in the Pose box (double click resets); Edit menu entries Ctrl+]
and Ctrl+[ step the pick one place. Built, not run, no tests per owner.
Next: owner tries it by eye.

Before that (2026-10-14, branch `joint-end-snap`, cut from `storyboard`,
not pushed): joint snap fixes on the owner's word. An end snap now puts
the joint (and its circle's center) on the limb's end, where its middle
line leaves the drawing, not one radius in; a joint circle there takes
the limb's width near the end. While a joint is dragged, the limb's
middle line (dashed), the line across and a ring at the spot show,
faint for the nearest spot until it catches. A joint circle draws under
both pieces it joins. Shape measuring moved to `anim/limb_mask.cpp`.
Built, ctest as before, not seen by eye. Next: owner tries it.

Before that (2026-10-14, branch `storyboard`, cut from `voice-gen`, not
pushed): the storyboard layer, finished from the WIP draft layer
(`wip/ux-wp3-draft-layer`, merged in, then reworked). A storyboard is a
kind of layer in the shot's stack (`StoryboardLayer`, `model/storyboard.h`):
raster cels, each starting at a shot frame and holding to the next, laid
over the canvas at its place in the stack, hidden by the layer's eye,
ordered in the Cast layer list (Add > Storyboard). Drawing reuses draw
mode through a `DrawJob`: Alt+N a blank frame at the playhead (the frame
before shows as a faint red onion, D toggles), Alt+Shift+N draws over the
frame showing; both are Edit menu entries, so the keys sheet and Find a
command list them. With no storyboard layer, Done makes one. A dashed row
per storyboard layer on the Pose timeline (pick, drag to retime, Delete,
double click, right click), a Storyboard box in the inspector (opacity,
In export for an animatic, draw buttons); shown on the Pose stage and the
Video preview, exported only with In export. Every edit is one named undo
step ("Draw storyboard frame"). Frame pictures are PNGs in "<project>
storyboard" beside the file, saved relative in the project file, and Save
As copies them along (`StoryboardManager`, `edit/storyboard_edits.cpp`,
`io/storyboard_paths.cpp`, `DocumentManager::CarryStoryboard`). Tested:
`storyboard_tests` (15 passed), `storyboard_render_tests`,
`storyboard_menu_tests`; the rest of ctest is as before (lint, the
palette-assert ui tests, scrub, video_page, and the gcc 13 test
compiles fail as they did). The app was not launched (owner's rule).
Next: owner tries the storyboard and Voice by eye.

Before that (2026-10-14, branch `voice-gen`, cut from `timeline-consistency`,
not pushed): a Voice tab in the Video tab's side panel, always shown:
clone a voice from a sample (file, or what the PC plays through `parec`)
or design one from a description, say a line, listen, and Add to timeline
at the playhead; saved voices across projects. The worker is
`tools/voice_gen/voice_gen.py` (run `tools/voice_gen/setup.sh` once;
CMake copies the script beside the binary). `VoiceGenManager`,
`VoiceLibrary`, `VoiceWorker`, `DesktopRecorder`, `ui/voice_panel.cpp`.
Then: the panel restyled (mode tiles, numbered steps, phosphor wave
screens to click and listen, LED meter, big red Generate; owner liked
it), Whisper writes a fresh sample's words by itself (Transcribe
button too), the three voices of `../TTS` copied into the library.
Chatterbox and IndexTTS-2.5 were added as clone models, then reverted
on the owner's word (Qwen sounds better); the Clone and Design switch
is plain buttons now. The worker's jobs were run by hand and work; the
app was not launched (owner's rule). Separate
branch `preview-speed` (worktree): Full, Half, Quarter preview quality,
a frame cache and frames drawn ahead; built, never run. Known: the app
crashes on close in `PalettePanel`'s destructor (older than this work).
Next: owner tries Voice and preview-speed by eye; fix the close crash.

Before that (2026-10-14, branch `timeline-consistency`, not pushed): the
Pose, Video and Comp timelines now share their view and chrome
(`ui/timeline_scale.h`, `timeline_ruler.h`, `timeline_keys.h`): one zoom
and scroll rule (wheel scrolls, Ctrl+wheel zooms at the cursor, Ctrl+=,
Ctrl+- and Ctrl+0, Show all), a frame ruler with a tick and number at
each second, one playhead with its frame tag and settle, the Scrub and
Animation switch in every ruler corner, paging along a playing
playhead, Alt+Left and Alt+Right nudge on all three, and Comp drags on
`DragFeedback`. Built, not seen by eye, no tests run (owner's rule).

Before that (2026-10-14, branch `comp-clip-edits`, cut from
`comp-effect-browser`, not pushed): Comp effect clips got the Video
tab's clip edits: S splits the picked clips at the playhead, Ctrl+X
cuts, Ctrl+C copies many, Ctrl+V pastes at the playhead keeping their
spacing (another shot's copy takes the stage pick), Ctrl+D duplicates
all picked, all in the timeline's right click menu too
(`edit/comp_cut.cpp`). No ripple remove: each effect has its own row.
Built, not seen by eye, no tests run.

Before that (2026-10-14, branch `comp-effect-browser`, cut from
`fix-comp-crash` since main lacks the Comp work, not pushed): the Comp
tab rebuilt. Left, every effect under a search field with hover help,
double click adds to the stage pick; right, the pick's effects (the
screen's when nothing is picked) as After Effects folds, shot Light
under them; stage clicks pick with a red outline. Applies to tiles
gone. Then copy and paste (Ctrl+C, Ctrl+V, menus), drag to reorder
plates, drag from the library onto the list or a doll on the stage,
and a polish pass (plate click opens, arrow at right, bigger on
square, color marks, groups, no-match hint, new effect scrolls in).
Then (same branch): palette rebuilt (color wheel, Recent, Doll folds by
use, eyedropper Pick), recently used effects, the agent's draw order
fix, effects as clips on the Comp timeline (every shot's, named by
target, picking, box, snapping, group drags, nudge, zoom), screen
effects run over the reel picture by their clip's extent (past cuts,
rim light keeps working), side list by playhead, Pose stage draws no
effects. Built, not seen by eye, no tests run (owner's rule).

Before that (2026-10-13, branch `ccr-33af7348-m78soh`, pushed, not
merged to main): the UX polish pass, from
`docs/redesign/ux-polish-plan.md` (read it first: the problems found,
the style kit, the work packages). Built and merged: WP0 the panel kit,
screen fit and pick rule (`ui/panel_kit.h`, `ui/screen_fit.h`,
`ui/pick_keys.h`, `ui/pick_list.h`, `ui/flow_layout.h`); WP2 the Video
tab (record sound with `RecordManager`, New shot here, double click
opens a shot via `VideoPage::OpenShotAsked`, a side panel only for the
pick, track scroll, wrapping bar); WP4 the inspector rebuilt with the
kit and showing only what the pick needs; WP5 one pick rule on the
canvases, timeline, strip, lists and references, Cast and Rig panel as
plated sections. None of it was seen by eye yet; recording never ran on
a real microphone.

Not finished, saved as WIP branches on GitHub (stopped by a usage
limit; each holds a partial, unbuilt commit on top of this branch):
- `wip/ux-wp1-window-shell`: WP1, the window fits any screen
  (`FitWindowToScreen`, `RememberLayout`, `GoldenDocks`), golden splits,
  Cast and Inspector on Pose only, tab order Video, Pose, Rig, Comp,
  Export in the top bar, wire `OpenShotAsked` to Pose and a key back,
  dialogs sized with `FitDialog`.
- `wip/ux-wp3-draft-layer`: WP3, one raster draft per shot made of
  cels (sketch the plan of a shot to its timing), drawn with the draw
  mode brush, a dashed Draft row, onion, opacity, eye, not exported.
To finish one: check out the WIP branch, read its diff and the plan,
merge `ccr-33af7348-m78soh` in, finish, build, then merge it back.

Build here (cloud): Qt 6.8 and ffmpeg 7 come from conda in
`/opt/qtenv`; configure with `PKG_CONFIG_PATH=/opt/qtenv/lib/pkgconfig
cmake --preset debug -DCMAKE_PREFIX_PATH=/opt/qtenv`. If a new Q_OBJECT
header links with "undefined vtable", delete that module's
`*_autogen` folder in the build dir and build again. About 31 test
programs do not compile with gcc 13 (missing-field-initializers on
`ArtPiece` in tests) and 5 ui tests abort on the palette assert in
`tests/ui/bench.h` (no `PaletteManager` there): both older than this
session. Tests the owner will need to update: `video_page_tests`
DraggingAnEndTrims (now resizes the shot), `collider_ui_tests`
(colliders now live per drawing).

Before that, the same branch (2026-10-12 to 13): every planned feature
in `docs/features` got a first pass (each file says what is left):
editing outside the frame, Smart Dials, Draw on a piece, Joint snap and
joint circles, Skins, Smooth warp, Objects, Resizing shots on the reel,
Transparent background, Sound on the reel, Text and subtitles,
Reference on the Pose tab, Compositing. Plus owner fixes: draw keys (B,
E flips the eraser, F fill, Alt or I pick, D onion), N starts a blank
drawing, the palette (square, drag anywhere, corner resize, remembered),
inspector hidden on Rig, rig settings scroll and never clip, zoom and
pan on the rig view and stage, side-named twin groups (hair_L, hair_R),
colliders per drawing, the shot strip scrolls. Details of each:
(2026-10-12, branch `ccr-33af7348-m78soh`, cut from
`claude/next`): editing outside the frame (docs/features/
off-frame-editing.md): the stage draws the shot past the frame at 40 %
with a darker surround and a frame line, O hides it (remembered per
project file), Shift+3 fits all, a red arrow points to picks out of
view, Shift+2 now frames the pick in the right place. Then Smart Dials,
first pass (docs/features/smart-dials.md says what is left): a Smart
dials fold in the Pose panel, template tiles, Set stop, dial keys on
the doll row, `DialManager`. Then Draw on a piece, first pass
(docs/features/draw-on-a-piece.md says what is left): N, Shift+N, right
click or the Drawing picker's "+" entries open draw mode over the
stage (pixel brush, eraser, fill, picker, R and onion, stroke undo);
Done saves a PNG in the doll's `snapper` folder, listed in rig.json,
as one undo step (`DrawManager`). Then Joint snap, first pass
(docs/features/joint-snap.md says what is left): a lone joint dragged on
the rig canvas catches on the width middle, an end circle's center or
the overlap with its parent (`anim/joint_snap.cpp`), Ctrl drags freely,
and right click Snap to shape or Snap all joints moves joints in one
undo step; J turns a joint circle on (limb-wide disc under the child,
drawing's color, recolored by a palette pick, saved in rig.json). Then
Skins, first pass (docs/features/skins.md says what is left): a Skins
box in the Rig tab makes skins and, with one picked, the Drawing picker
sets or hides pieces in it; a Skins fold in the inspector wears them at
the playhead on their own keyed track (Shift click stacks, Shift+[ and
] step), applied by the shared sampler (`SkinManager`). Then Smooth
warp, first pass (docs/features/smooth-warp.md says what is left): a
Look box in the Rig tab (Crisp, Smooth, Smooth flat, Hard edges) and a
per-piece Keep crisp; smooth pieces draw from a Scale2x upscale kept in
the WarpCache, blended, and hold backslash shows Crisp. Then Objects,
first pass (docs/features/objects.md says what is left): PNG and GIF
props as one-piece dolls in the library (an Objects row in the Cast
panel, files dropped on it or on the stage), GIF play modes on the shot
clock in an Object fold, and right click Hold with a doll piece, Drop
here and hand-offs, following place, turn and scale
(`ObjectManager`, `LinkSolver::HeldMove`). Then Resizing shots on the
reel, first pass (docs/features/reel-shot-resize.md says what is left):
dragging a full shot clip's edge changes the shot's own length (Alt
trims, Shift ripples, Ctrl no snap, Edges button for Hold or Stretch),
other full clips follow, and a double click in a gap offers Fit shot to
gap (`ReelManager::ResizeShots`, `edit/shot_timing.h`). Then
Transparent shot background, first pass (docs/features/
transparent-background.md says what is left): a Transparent tile in the
Shot box and a Transparent start, a checkerboard on the stage (Shift+V
previews over a color), see-through clips over lower reel tracks, and
new export formats PNG frames, WebM and MOV with alpha
(`media/video_formats.h`, `ShotManager::SetTransparentAll`). Then Sound
on the reel, first pass (docs/features/reel-audio.md says what is left):
sound tracks under the picture tracks with Mute, Solo and a fader,
dropped sound files with waveforms, a volume line and fades, the song
as a clip on old projects, the mix in playback and export (plus Sound
only WAV), and shots on the reel play the reel's sound under their clip
on the Pose tab (`edit/reel_sound.cpp`, `SoundManager`). Then Text and
subtitles on the reel, first pass (docs/features/
reel-text-and-subtitles.md says what is left): text clips (T+ or Ctrl+T,
typed in place, dragged and scaled on the preview, a Text tab for look,
palette colors, place tiles, In and Out, linked styles) and a subtitle
track timed by tapping L, with a cue list, find and replace, SRT and
WebVTT import and burned in or side file export (`edit/reel_text.cpp`,
`edit/reel_subtitles.cpp`, `render/reel_text.cpp`). Then Reference on
the Pose tab, first pass (docs/features/pose-reference.md says what is
left): videos, GIFs and pictures linked into a shot (shot menu or Alt +
drop), drawn behind or over the layers on the Pose stage only, with a
dashed row each on the timeline (drag the start, Alt slides the video),
a References fold (opacity, tint, fit, speed, markers), Y and M
(`ReferenceManager`, the renderer's `under` picture). Then Compositing,
first pass (2026-10-13, docs/features/compositing.md says what is
left): a Comp tab with a per-shot stack of nine anime finishing effects
(rim light, transmitted light, gradients, color swap, line smoother,
motion blur, sparkle, speed lines, finish) that target a layer, pieces
or picked colors, with 19 shared blend modes, keyed knobs, a shot
Light, V, K and I, run by the one renderer after every layer
(`CompManager`, `render/comp.cpp`, `render/blend.cpp`). No tests yet,
the owner adds them later. Every planned feature in docs/features now
has a first pass.

Before that (2026-10-12, branch `inspector-physical`, cut from `ui-test`): the
inspector as a hardware panel (pose box: dials, pad, faders; golden ratio,
raised plates over sunken frames; other boxes with faders; dropdowns
sunken; dials and sliders take the wheel, reset on a double tap and wait
for the panel to stop scrolling); boxes show only what the pick needs;
undo flash on the stage; transport above the timelines; Ctrl+Shift+R
restarts; wheel ticks and live edits fold into one undo step; warp dot
settings per layer of a shot with a doll default and a Set as default
button; groups inside the warp dots panel; Ctrl+Shift+C and V copy and
paste a part's settings; undo and redo per shot; two colliders squash
alike; `anim` and `render` are -O2 in Debug. Owner tests each by eye.
Open: the Rig tab still edits the doll default only; orphan `layer_dots`
of deleted layers are never pruned; copy and paste skips layer name and
timing; the physics still re-runs from frame 0 on each drag move.

Earlier: Pose box redesigned as a hardware panel (dials for turn/lean/swivel, a pad for move, faders for scale/skew/opacity, fields kept as readouts); other inspector boxes are still plain forms, next to convert; undo flash on the stage (changed layers flash white, old place fades); play buttons moved from the top bar to above the pose timeline and into the video bar (`TransportBar`, none on Rig); owner said no more tests from now on; redesign
steps 8 to 13, first pass (see next steps, item 3); motion presets and the Motion box removed; mirror flips the
mesh and makes twin groups.

Also: jelly rebuilt
as one smooth blob per clump of dots (shift plus squash, stretch and turn
springs, dots follow it, weaker near pins; from the softbody research).
Before it, a per-dot version: The fit, area refill and fold fixes are gone; jelly is the
cloth move plus a spring to the neighbours' average swing, firmer hold
near pins, a minimum side length and a soft one-radius limit. Not seen
in the real app yet: owner tests the feel and says what is off (period,
squish and droop were retuned to a probe, not by eye). Cloth is not
changed yet; same idea next if the jelly feels right.

Also today: dot groups.
Pick dots, press Group in the Groups box (shown while a doll is picked),
name it; a chip then picks the whole set (Shift adds, Ctrl flips) and
the Warp dots tiles set Still or Jelly on all of it. Groups belong to the
shot; right click a chip to rename, add or remove the picked dots, make
it a default (kept on the doll, shown in every shot with it) or delete.
Owner to try it and say how it should feel (chip look, naming, order).

Earlier the same day: jelly dots
may no longer stray past one blob radius from where the body carries
them (Pudding and Water balloon scattered to 3 blobs when shaken hard).
Not seen in the real app yet: owner to retest; if it still tears, the
cause is in the fold fixes (cloth_parts.cpp), not the cap.

Before that (2026-10-10, branch `workflow`, merged into `ui-test`):
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

0. Finish WP1 and WP3 from their WIP branches (see above), then the
   owner tests the whole polish pass by eye and says what feels wrong.
1. The owner tests the latest `ui-test` build (the new jelly, groups,
   the key chip bar, corner switch, dials, inspector title); fix what
   they find, one change at a time. The owner said: no tests for visual
   things, they test those by eye; no test rule in this file now.
2. Performance: a Release drag of a doll with jelly limbs takes about
   15 ms a move; the lag was the Debug build (90 ms), so `anim` and
   `render` are now -O2 in Debug too (23 ms). Next: profile a real
   project, and the physics still re-runs from frame 0 on every drag move.
3. Redesign (docs/redesign/index.html section 12; keep the UI fun,
   simple and game like): steps 8 to 13 are built in a first pass (key
   chip bar, corner switch, inspector title, dials, folds, motion tiles,
   stage zoom/pan/fit, guides, nudge, Tab, doll cards and drag to stage,
   shot chip pictures, transition badges, rig rope and pivot snap, find
   a command, keys sheet, burst, sounds, new project tiles, export
   buttons). Not built yet, in the doc: drag rows for draw order,
   IK bones and click the elbow to flip, the inline gap picker, the
   transition popover with tiles, layer rows with eye and kind icon and
   rename in place, the "+" tiles, hit-stop, input buffering. (Undo
   flash is built on the stage; pieces do not slide back yet.) Owner tests each by eye and says what feels wrong.
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
