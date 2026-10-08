# Snapper architecture

Snapper is a puppet animator for music videos in the style of Execution
Clap. Dolls are drawn in Krita, rigged and posed in Snapper, cut into
shots, dressed with camera moves, text and effects, and exported as MP4
or GIF. Everything is finished inside Snapper.

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
| `model`  | Plain data: dolls, project, shots, keys           | base          |
| `anim`   | Pure math: sampling keys, pose, IK, warp, presets | model         |
| `io`     | Reading and writing doll and project files        | model         |
| `media`  | FFmpeg and audio: decode, play, encode            | base          |
| `render` | One frame of the master track to pixels           | anim          |
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
- `media` knows nothing about dolls or shots. It decodes audio, plays
  it, and encodes frames it is handed.
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
  audio and video files are read or written (`media`).
- Keys hold by default (step). A key can instead ease or move linearly
  into the next one.
- Poses are usually on 2s or 3s; the timeline offers both as a hold
  length, it does not force them. Picked keys can be spaced evenly on
  1s to 4s (`KeyManager::Space`); keys after them slide along so later
  timing stays.
- The timeline's corner flips stepping between scrub mode (the arrow
  keys move one frame) and animation mode (they jump to the next frame
  where the picture changes: a key, a shot's start, or each frame of
  an ease; `PoseChanges`).
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
- a camera (pan, zoom, rotate, shake),
- a transition into the next shot (cut, swipes, flash, crossfade).
  The next shot starts that many frames early and the two are mixed.

An effect layer (flash, fill, invert, zoom punch, halftone, glitch,
posterize) changes everything drawn below it, like an adjustment
layer. Effects are drawn in screen space, after the camera.

Every animated value is a channel of keys, and every channel uses the
same key type and the same sampling code, whether it moves an arm, the
camera, or a glitch amount.

## Rendering

`FrameRenderer` draws on the CPU with `QPainter` into a `QImage`, so it
runs the same in the window, in an export thread and in a headless
test. Warped pieces are drawn by `WarpImage`, which maps each grid cell
as two triangles back into the drawing. Each renderer has its own
`ImageCache`; nothing is shared across threads. Preview may render at a
smaller scale; effects scale their sizes with it so they look the same.

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
| `RigManager`           | Rig edits: parent, pivot, order, IK, warp grid |
| `ShotManager`          | Shots on the master track, transitions         |
| `StageManager`         | What is in a shot: actors, props, text, effects |
| `SelectionManager`     | What is picked: shots, layers, pieces, keys    |
| `PoseManager`          | Pose edits, IK drags, copy, paste, mirror      |
| `KeyManager`           | Keys on the timeline: move, space, ease, retime |
| `PresetManager`        | Motion presets and saved poses                 |
| `PlaybackManager`      | Playhead, play, pause, loop, song loading      |
| `ExportManager`        | MP4 and GIF export on a worker thread          |

A manager that grows past one concern is split, not grown; a file stops
at 400 lines.

## Picking many

Every list of things (stage pieces and layers, the layer list, timeline
keys, shots, rig pieces, library dolls) picks the same way: click picks
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
and saved poses for picked pieces or layers; the drag node panel for
a picked warp dot (`Inspector::Refresh`).

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
- `media` is tested by encoding a clip and decoding it back.
- CTest also runs the Snawy's Law linter and the layer check.

## Folder layout

```
src/<module>/        code, one manager or concept per .h/.cpp pair
tests/<module>/      that module's tests
tools/               build checks (layer check), Krita exporter
cmake/               shared CMake functions
```
