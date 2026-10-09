# Snapper architecture

Snapper is a puppet animator for music videos in the style of Execution
Clap. Dolls are drawn in Krita, rigged and posed in Snapper, cut into
shots, dressed with camera moves, text and effects, laid out with video
files (a speedpaint) on the reel, and exported as MP4 or GIF. Everything is finished inside Snapper.

This file is the map. Each section is a rule the code keeps. When code
and this file disagree, one of them is a bug.

## Modules

Every module is a folder under `src/` and its own library. A module may
include only itself and the modules below it in this list. A test in
CTest (`layers`) fails the build when an include breaks that rule; the
graph it checks is the one `CMakeLists.txt` declares, so there is one
source of truth.

| Module   | Owns                                              | Uses          |
|----------|---------------------------------------------------|---------------|
| `base`   | Ids, frame time, errors, `Tr` for messages        | Qt Core       |
| `model`  | Plain data: dolls, project, shots, keys, reel     | base          |
| `anim`   | Pure math: sampling keys, pose, IK, warp, presets | model         |
| `io`     | Reading and writing doll and project files        | model         |
| `media`  | FFmpeg and audio: decode, play, encode, read video | base         |
| `render` | One frame of the master track or reel to pixels   | anim          |
| `edit`   | Managers: every change to the project             | render, io, media |
| `ui`     | Widgets: show state, turn input into manager calls | edit         |
| `app`    | `main`, the composition root                      | ui            |

What each layer may not do:

- `model` has no behaviour beyond checking its own invariants. No
  signals, no files. From Qt Gui it uses value types only (`QColor`).
- `anim` is pure: data in, data out. No state, no Qt Widgets, no I/O.
  This is where the hard math lives, so it is where most tests live.
- `io` turns bytes into model values and back. It never decides
  anything about editing.
- `media` knows nothing about dolls or shots. It decodes audio and
  video files, plays audio, and encodes frames it is handed.
- `render` draws; it never edits. Preview and export call the same
  renderer, so what you see is what you export.
- `edit` holds all editing logic. A rule about how keys move, how a pose
  mirrors, or when a button is allowed lives here and nowhere else.
- `ui` holds no editing logic. A widget asks a manager, shows the
  answer, and forwards input. If two widgets would need the same rule,
  the rule belongs in a manager.

## The one edit path

There is exactly one way the project changes:

1. A widget calls a manager method, for example
   `PoseManager::Rotate(track, frame, degrees)`.
2. The manager reads the current project from `HistoryManager`, builds
   the next project value with the shared helpers in
   `edit/project_edits.h` (`WithShot`, `WithLayer`, `WithTrack`) and
   hands it to `HistoryManager::Apply` with a label such as "Rotate".
3. `HistoryManager` stores the old value for undo and emits `Changed`.
4. Widgets repaint from `HistoryManager::current()`.

Nothing else writes to the project. That gives undo, redo, dirty state
and repaint for every feature without each feature writing them again.

A drag is one undo step: the tool opens an `EditScope` and calls the
same manager methods on every mouse move. While a scope is open,
`Apply` only previews; the scope lands everything as one step when it
goes out of scope, named after the drag. `Cancel` (Escape) puts the
start back. Manager methods take absolute values (an angle, not a
nudge), so calling them again and again during a drag is safe.

Every channel of keys is reached the same way: a `TrackRef` names it
(a layer's own move, a doll piece, the camera, or an effect's
strength), and `VisitTrack` / `ReadTrack` hand over the channel
whatever its value type. Posing, keys, presets and the selection all
share this.

## Project data and undo

The project is a value. Undo keeps whole snapshots, not hand-written
inverse commands, so an undo bug cannot exist per feature.

Snapshots stay cheap through sharing: big parts of the project (each
shot, each doll rig) are held as `std::shared_ptr<const T>`. An edit
copies only the part it touches and shares the rest with the previous
snapshot. Drawings are never in the project, only their file names, so
pixels are never copied.

History keeps at most `kMaxUndoSteps` steps and drops the oldest.

## Time

- One clock: whole frames at 24 fps (`Frame`). Seconds exist only where
  audio and video files are read or written (`media`), and as a way to
  type a shot's length, which becomes the nearest frame
  (`FramesNearSeconds`).
- Keys hold by default (step). A key can instead ease or move linearly
  into the next one.
- Poses are usually on 2s or 3s; the timeline offers both as a hold
  length, it does not force them. Picked keys can be spaced evenly on
  1s to 4s (`KeyManager::Space`); keys after them slide along so later
  timing stays.
- A timing chart sets the spacing of the in-betweens between two
  frames A and B without moving any frame or key in time. The math is
  pure (`anim/timing_chart.h`): presets to fractions, tick drag
  clamping, and `BakeChannel`, which writes a key on every frame from A
  to B at its fraction of the way from the pose A has to the pose B has.
- The timeline's corner flips stepping between scrub mode (the arrow
  keys move one frame) and animation mode (they jump to the next frame
  where the picture changes: a key, a shot's start, or each frame of
  an ease; `PoseChanges`).
- The playhead runs on one of two timelines: the shots one after
  another (Pose and Rig tabs) or the reel (Video tab). Each keeps its
  own place, and posing always works at the shots' playhead
  (`PlaybackManager::FrameOn`). On the reel, animation mode jumps from
  cut to cut (`ReelCuts`).
- Playback is driven by the audio clock, so picture never drifts from
  the song. Without a song or a sound card it falls back to elapsed
  time.

## Dolls

A doll is a folder in the doll library:

- `art.json` and the PNGs come from Krita (`tools/krita` holds the
  exporter). Snapper only reads them. Old dolls (a single
  `doll.json` from the first Snapper) are converted to `art.json` and
  `rig.json` when the library is scanned, rig and rest angles included;
  the old file stays as `doll.json.old`.
- `rig.json` belongs to Snapper: parents, pivots, draw order, IK chains,
  warp grids, default drawing per piece.

Because Krita and Snapper write different files, re-exporting from
Krita never touches the rig. When Snapper loads a doll it matches rig
pieces to art pieces by name: new art pieces get a default rig, rig
pieces whose art is gone are reported (`ReconcileReport`), never
silently dropped.

A project carries a copy of every doll it uses, so it opens the same
even if the library changes. `DollLibraryManager::Reload` takes newer
art into the project, and `SaveRig` writes the project's rig back to
the library.

In Krita, a group is a piece and each child layer is one of its
drawings (head: happy, angry...). The visible child is the default. A
plain layer is a piece with one drawing.

A doll is keyed as a whole, as a drawing is in traditional animation:
posing any piece keys every piece and the layer's own move at that
frame, so one key on the doll's timeline row is its full pose.

A piece can:

- rotate, move, scale (separately in x and y) and skew,
- swap between its drawings,
- warp locally through a grid of handles, so one-piece parts such as a
  trunk can still crease at the collar or chest.
  On the stage a picked piece shows its grid as faint lines with
  yellow dots; dragging a dot keys that point's push. Each dot has a
  rubber reach in grid cells, kept in the rig (0 by default): pulling
  the dot drags its neighbours along, fading out at the reach. Clicking
  a dot picks it (Teto red, with its reach drawn) and the wheel then
  sets the reach instead of turning the piece.
  Pressing D on a picked dot makes it a drag node (ringed): a spring
  that trails where the pose and the layer carry it, then bounces back,
  like hair or a chest, spread by the dot's reach (`DraggedPoses`).
  Its Lag and Bounce are in the Drag node panel. The spring runs from
  the shot's first frame, so every render of a frame is the same, and
  shows only on the doll's own beat (its keys, each frame of an ease,
  the same spacing through a long hold), so a doll on 3s drags on 3s.
  A dot can also move by itself (a point motion: wave, pulse,
  breathe, sway, shiver), with a size in pixels, a cycle in frames, a
  direction and a delay. Its neighbours follow by its rubber reach, as
  they follow a drag node, and a wave reaches them later the further
  they are. Its push is a formula of the frame (`PointMotionPushes`),
  added on top of the keys and drag nodes on the same beat, so it
  never needs keys and renders the same every time. It is set in the
  Dot animation panel under the Drag node panel (no key: a motion is
  one of five kinds, so the list is the one click), and the dot wears
  a yellow square. A dot can have a drag node, a motion, or both.
- turn in depth with the whole doll: a doll picked whole has two
  yellow handles (and Lean and Swivel fields). Lean, beside the box,
  turns it like a wheel facing the camera; Swivel, below it, like a
  door on an upright hinge. Both are keyed angles on the layer's own
  move with no limit, drawn on top of the pieces' keys (`ShownPoses`,
  `LeanPoses`), so turning back always gives the posed doll again.
  Nearer pieces grow and spread, spacing and drawings squash along
  the turn (drawings less, and not at all for pieces ticked "Keep
  shape when leaning" in the rig), past a quarter turn the doll is
  upside down or seen from behind, and a piece clearly nearer than
  its parent, child or sibling draws in front of it.
- belong to a two-bone IK chain. IK is a posing tool: dragging a hand
  solves the chain and writes ordinary rotation keys. Playback only
  ever plays keys, so what was posed is what plays.

## Shots

A project is one song and an ordered list of shots on a master track.
Each shot has its own stage:

- a background colour, and layers bottom to top: dolls, pictures,
  text for lyrics, and effects,
- a camera (pan, zoom, rotate, shake).

On the master track the shots follow each other with plain cuts;
transitions belong to the reel.

An effect layer (flash, fill, invert, zoom punch, halftone, glitch,
posterize) changes everything drawn below it, like an adjustment
layer. Effects are drawn in screen space, after the camera.

## Links

A layer or a doll piece can follow another thing's movement: right-
click the leader on the stage with the followers picked. A `Link` in
the shot names the follower, the leader, the frame it was made at and
a strength. From then on the follower copies strength x how far the
leader's point (a layer's origin, a piece's joint) has moved in shot
space since that frame, on top of its own keys, so it never jumps onto
the leader. Only x and y are copied.

`LinkSolver` (anim) works this out: a layer's share shifts it in shot
space; a piece's share is turned into its parent's space and added to
its offset, so it carries its children. A leader that follows something
passes that on, so chains work. Everything that draws or hit-tests the
stage asks the shot's solver (`RenderShot`, `HitTest`, the stage
geometry), so what is clicked is what is seen. Keys stay as keyed; the
pose tool moves by differences, so a linked thing drags without
jumping.

`LinkManager` refuses links within one doll (its pieces already move
together) and links that would loop back into themselves (`WouldLoop`);
a loop in a file counts as no push. Links drop with their layers and
follow a duplicated shot's own layers. Drag nodes feel links: the
solver hands `DraggedPoses` how far links carry each drag-node piece
frame by frame (`LinkShifts`), so a linked doll's warp points trail on
its own beat as if it had moved by itself.

Every animated value is a channel of keys, and every channel uses the
same key type and the same sampling code, whether it moves an arm, the
camera, or a glitch amount.

## The reel

The reel is the final video: tracks of clips, bottom to top, cut on the
Video tab. A clip shows a stretch of its source from `in` for `length`
frames, starting at reel frame `start`. Its source is either

- a video file (a speedpaint, a recorded take), played as it is, or
- a shot, played as a finished video. A shot clip is a link, not a
  copy: fixing the shot on the Pose tab fixes every clip of it. A shot
  made shorter since leaves the clip's tail empty (hatched); a removed
  shot leaves an empty clip. Nothing inside a shot is edited on the
  reel.

Clips on a track are sorted and never overlap (`HasRoom`). A frame
shows each track's clip, bottom first, so a shot on a higher track
covers the speedpaint below while it plays (`ReelAt`). The song plays
under the reel as it does under the shots. Once the reel has a clip it
is what gets exported; until then the shots are, one after another.

Drags on the reel snap to other clips' cuts, cut markers, the playhead
and frame 0 (`SnapDelta`, `SnapFrame`).

The reel's timeline edits like the shots' one: clips pick as every
list does (a box picks what it touches, `ClipsIn`), drags move and
trim them, S splits at the playhead (the picked clips, or with none
picked whatever the playhead is in, `SplitTargets`), Delete removes,
Shift+Delete removes and closes the gap on each track
(`RippleDeleteAll`). Ctrl+C, Ctrl+X and Ctrl+V copy, cut and paste:
`ReelManager` keeps the clipboard, and a paste lays the copies down at
the playhead on the tracks they came from, keeping their spacing.
Ctrl+D lays copies right after the picked clips. The helpers these
edits share are in `edit/reel_edits.h`.

A transition (swipes, flash, crossfade) lives on the cut between two
clips that touch on one track, stored on the left clip (`Clip::out`).
The two are mixed over the last frames before the cut, so the reel's
timing never changes; the right clip shows the frames leading up to
its in point there, holding its first frame where its source has
none (`ReelAt`). Its length is bounded by both clips, each keeping a
frame of its own (`UsableLength`). Pulled apart, the clips just cut;
touching again, the transition is back. Splitting a clip leaves the
transition on the right half. Double-clicking the cut picks it
(`TransitionPicker`, `ReelManager::SetTransition`). Files from before
kept a transition on each shot into the next one: on load it moves
onto the cut where a clip of that shot meets the next clip on its
track, and is dropped where there is none (`MoveShotTransitions`).

Cut markers are where cuts should fall, marked by ear: Mark cut (M)
drops one at the reel's playhead while the song plays, or takes away
the one there. The reel's playhead runs at least the song's length, so
cuts can be marked before any clip is placed. Markers are saved with
the reel (`cut_markers` in the project file).

The stretch between two markers (the first starts at frame 0) is a
gap of fixed length (`SlotAt`). Double-clicking a track inside one
asks which part of a video plays there (`SlotPicker`): it starts from
what the track's video would play there (`SlotFillAt`) and a slider
slides the video under the gap. `ReelManager::FillSlot` re-picks the
clip already filling the gap, or cuts away what is under it and lays
a new clip over it, as one undo step.

## Rendering

`FrameRenderer` draws on the CPU with `QPainter` into a `QImage`, so it
runs the same in the window, in an export thread and in a headless
test. Warped pieces are drawn by `WarpImage`, which maps each grid cell
as two triangles back into the drawing. Each renderer has its own
`ImageCache`; nothing is shared across threads. Preview may render at a
smaller scale; effects scale their sizes with it so they look the same.

`RenderReel` draws a reel frame: shot clips through `RenderShot`, video
clips fitted to the canvas, keeping their shape. Over a transition both
clips are drawn over the tracks below and mixed by `Combine`. Pictures
of video files come through the `VideoFrames` hook, so drawing never
opens a file; the `edit` side's `VideoFileFrames` reads them with
`VideoReader`, keeping each file open so playing forward decodes each
picture once. Like the renderer, each thread has its own.

The CPU is enough for flat-colour dolls at 1080p. If preview at full
size ever drops below 24 fps, the upgrade is to move `FrameRenderer`
onto the GPU behind the same interface; nothing else would change.

## Managers

One manager per concern, each owning its state and reached through a
small interface. `app` builds them once, in dependency order, and they
are destroyed in reverse.

| Manager                | Concern                                        |
|------------------------|------------------------------------------------|
| `HistoryManager`       | Current project, undo, redo, dirty state       |
| `DocumentManager`      | New, open, save, autosave, recent files        |
| `DollLibraryManager`   | Doll folders, loading, reload on re-export     |
| `RigManager`           | Rig edits: joints, IK, warp grids, their dots  |
| `ShotManager`          | Shots on the master track: order, length, look |
| `ReelManager`          | The reel: clips, tracks, cuts, transitions     |
| `StageManager`         | What is in a shot: actors, props, text, effects |
| `LinkManager`          | Which things copy another's movement, how much |
| `SelectionManager`     | What is picked: shots, layers, pieces, keys, clips |
| `PoseManager`          | Pose edits, IK drags, copy, paste, mirror      |
| `KeyManager`           | Keys on the timeline: move, space, ease, retime, bake a timing chart |
| `TimingChartManager`   | The timing chart session: picked thing and frames to chart, ticks, live preview in one `EditScope`, Apply or Cancel |
| `PresetManager`        | Motion presets and saved poses                 |
| `PlaybackManager`      | Playhead on either timeline, play, loop, song  |
| `ExportManager`        | MP4 and GIF export of the final video, threaded |
| `MotionManager` (`ui`) | How long and how softly the UI moves: the Motion table, Full / Calm / Off, saved in settings |
| `ToastManager` (`ui`)  | Toasts: the queue, their time, stacking, dismissal and the Undo they offer |
| `TimingChartDialog` (`ui`) | The floating timing chart window (Shift+T, or Define timing chart... in the stage and timeline menus); its strip only draws the ticks and forwards drags to `TimingChartManager` |
| `RefusalShaker` (`ui`) | The shake and the reason on a greyed control clicked anyway |

`MotionManager` lives in `ui` because it only serves widgets and holds no
project state. Its table and curves (`motion_table.h`) are pure and
tested without a widget; every animation asks `MotionManager` for its
duration and curve, so the View > Motion switch changes all of them at
once. Qt has no reduced-motion query, so a fresh install starts in Full.

A problem the user caused is a toast, not a modal or status bar text:
widgets pass the manager's error text to `ToastManager::Problem` (the
window's problem signals all end there). `ToastQueue` (pure, tested
without a widget) holds at most three toasts for four seconds each,
oldest out first; the same text again refreshes instead of stacking.
`ToastView` floats them over the bottom edge of the pages, so nothing
jumps, never takes focus, fades through `MotionManager`, and a click or
Escape dismisses. A big action (a delete) calls `ToastManager::Finish`,
which on success shows a toast with an Undo button: the toast keeps the
`HistoryManager::revision` of the step it announces and Undo runs only
while that is still the latest step, so it can never undo something
else; a newer edit takes the button away. Pinned hints (rig canvas) and
file failures (warning boxes) stay as they are.

`RefusalShaker` watches the whole application: a mouse press on a
disabled widget (the "greyed with a reason" rule, `Explain`) shakes it
4 px (`ShakeOffsetPx`, pure, tested) over the Move time and, on release,
shows the reason beside the cursor (`RefusalReason`: its tooltip, else
the nearest parent's, else "Not available right now"). In Off it only
shows the reason. No control opts in. `Explain` sets the tooltip, then
the enabled state; every disabled control or menu item goes through it,
and a window test fails on any greyed control with an empty tooltip.

Number fields get their hands-on feel from three small pieces. The math
is pure and lives in `base`, tested without a widget: `EvalNumber`
(`number_text.h`: "*2", "+15", "90/2"), `HasMixedValues` (picks that
differ show "Mixed") and `ScrubDrag` (`scrub_drag.h`: dead zone, Shift
10, Alt 0.1, clamp, rounding). In `ui`, `ScrubLabel` turns a label drag
into `ScrubDrag` values on the field, so the field's `LiveEdit` makes the
drag one undo step (Escape cancels it), and `NumberInput` applies typed
math on Enter or leaving the field, flashing the field red and keeping
its value on bad text. `AddNumberRow` / `AddNumberPair` in
`form_helpers.h` give a field all of this with one call.

Handles look small and grab big, the same on the stage, the rig canvas
and the timelines. `grab.h` (pure, no widget) holds the one set of
numbers (drawn 8 px, grabbed within 12 px, 4 px dead zone before a drag,
100 ms grace for a handle that just hid, 15 % hover growth) and the one
hit-test: `NearestGrab` picks the nearest handle within reach, and
`IsPastDeadZone` says when a press has become a drag, after which the
handle jumps under the cursor. `HandleHover` holds which handle the
cursor is over and animates its growth through `MotionManager`; each
canvas owns one. `CursorFor` gives each kind of handle its cursor. No
widget keeps its own reach or drag threshold.

Drags talk the same way everywhere. `DragFeedback` (one per canvas,
drawing only) shows a value chip above and left of the cursor (one
placement rule, `ChipRect`, for drag and wheel chips: 24 px clear of
the arrow, flipping near edges), a faint copy of the dragged thing
where the drag began, a flash when it snaps and a pop where it lets
go. Flash and pop go through `MotionManager` (nothing in Off); the
ghost shows in every mode. Escape clears it at once. The words, the
chip place and the 15 degree turn snap are pure functions in
`drag_text.h`; a canvas picks the words and calls `Update`, `Settle`
or `Cancel`. As a drag is grabbed, the canvas calls `SetGhost` with
`MakeGhost` (`drag_ghost.h`), which runs the view's own paint code for
the item (`PaintBlock`, `PaintClip`, `PaintKey`, `PaintJoint`,
`PaintTip`, or `FrameRenderer::RenderLayers` on the stage) onto a
clear image drawn at low opacity.

Which keys and clips an edit made, took away or moved is worked out by
pure helpers in `edit/change_diff.h`, tested without a widget:
`DiffKeys` compares one shot's timeline rows before and after, and
`DiffClips` compares the reel's clips by id. A key that kept its pose
and changed frame is one move, not a removal and an addition. Views
only animate the answer; the data never waits for an animation.
`ChangeWatcher` (one per timeline) hands a view the project from before
each step that lands: a drag is one step, a cancelled drag, a save and
a newly opened file are none, and a step that returns to a state the
history already visited (`HistoryManager::revision`) is flagged as
undo or redo. `ChangeEffects` (`ui`, one per timeline) turns that into
decoration through `MotionManager`: a new key pops 1.4 to 1 with a
ring, a deleted one puffs out, what undo or redo brings back rewinds
(a bright flash settling from 1.25), and a key moved without a drag
(spacing, undo or redo of a move) or a reel clip moved the same way
slides from its old place on the Move curve. The timeline draws
these over the real project, so data and picking never wait for them;
in Off there are none, and the final state shows at once. The pose
timeline (`TimelineView`) animates keys per row; the reel
(`ReelTimeline`) animates whole clips, growing only 5 % as a clip is
long. Each timeline's playhead lands on a frame with a small overshoot
(`PlayheadSettle`: an offset to draw by, none while playing or for a
jump past `kMaxSettleFrames`, none in Off).

The look of every button and tab lives in one place: `SnapperStyle`
(`snapper_style.h`), a `QProxyStyle` over Windows that `theme::Apply`
sets on the application. Raised bevels, the hover brighten, the sunk
press and the chunky tab are drawn there, so no widget carries a
stylesheet. What a button looks like in each state is `LookOfButton`
(pure, tested). A widget that fades its hover sets the `kHoverProperty`
number (0 to 1); without it the style uses the plain mouse-over state.

The top bar (`TopBar`) holds the mode tabs (`ModeTabs`: a red bar
slides under the active tab through `MotionManager`), the transport
and the Undo and Redo buttons. It forwards clicks to `PlaybackManager`
and `HistoryManager` and decides nothing: the words, tooltips and
enabled states are `MakeStepFace`, `FrameCounterText`, `TimecodeText`
and `PlayText` in `bar_text.h` (pure, tested). The window computes the
Undo and Redo faces once from `HistoryManager` and shows them on the
menu entries and the buttons, so they always agree. Bar buttons never
take focus, so Space and the arrow keys stay on the menu entries.
`HoverFade` eases a button's `kHoverProperty` (hover, Hover time) and
`kPressProperty` (held down, Press time; springs back on release over
the Pop time) through `MotionManager`; `SnapperStyle` squashes a pressed
button by `SquashScale` (4 %). The tab icons are painted by `ModeIcon`
(no image files), `BumpLabel` hops the frame counter when a step
changes it, and `StepButton` slides the Undo and Redo words when they
change. None of them moves in Off.

A manager that grows past one concern is split, not grown; a file stops
at 400 lines.

## Picking many

Every list of things (stage pieces and layers, the layer list, timeline
keys, shots, reel clips, rig pieces, library dolls) picks the same way: click picks
one, Shift adds, Ctrl flips one in or out, a box on empty space picks
what it touches (Shift adds, Ctrl takes out), Ctrl+A picks all, Escape
or a click on empty space clears. `Combine` in `selection_manager.h` is
the one rule they share.

A change made with many things picked is one undo step and is added to
each thing's own value, so they keep their differences: x 10 and x 0
moved by 5 become 15 and 5. Managers offer these as `...All` or
`Shift...` methods; widgets show the focused pick's value and send the
difference.

The side panel shows only the settings of what is picked: the shot
and its camera with nothing picked on the stage; pose, layer, motion
and saved poses for picked pieces or layers; the drag node and dot
animation panels for a picked warp dot, and the Link box only while a
pick follows something (`Inspector::Refresh`).

## Errors

No exceptions. A call that can fail returns `Result<T>`
(`std::expected<T, Error>`), and the caller handles it. An error the user
caused is shown to the user in plain words. A broken invariant is an
`assert`.

## Limits

Every collection has a fixed maximum (`kMax...` next to its type), and
every loop is bounded by one. Id counters live in the project, so an id
is never handed out twice to things that exist at the same time; after
an undo, the undone thing's id may be given out again.

## Tests

- Each file in `tests/<module>/` is its own test program.
- `anim` and `io` are tested as pure functions.
- Managers are tested headless, through their public methods.
- `render` is tested by rendering offscreen and checking pixels.
- `media` is tested by encoding a clip and decoding it back, sound and
  pictures.
- CTest also runs the Snawy's Law linter and the layer check.

## Folder layout

```
src/<module>/        code, one manager or concept per .h/.cpp pair
tests/<module>/      that module's tests
tools/               build checks (layer check), Krita exporter
cmake/               shared CMake functions
```
