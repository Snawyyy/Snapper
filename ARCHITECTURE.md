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
source of truth. Modules are added to the build with their first real
code; the table already fixes where each future feature goes.

| Module   | Owns                                              | Uses          |
|----------|---------------------------------------------------|---------------|
| `base`   | Ids, frame time, errors                           | Qt Core       |
| `model`  | Plain data: dolls, project, shots, keys           | base          |
| `anim`   | Pure math: sampling keys, pose, IK, warp, presets | model         |
| `io`     | Reading and writing doll and project files        | model         |
| `media`  | FFmpeg and audio: decode, play, encode            | base          |
| `render` | One frame of a shot to pixels, effects included   | anim          |
| `edit`   | Managers: every change to the project             | render, io, media |
| `ui`     | Widgets: show state, turn input into manager calls | edit         |
| `app`    | `main`, the composition root                      | ui            |

What each layer may not do:

- `model` has no behaviour beyond checking its own invariants. No
  signals, no files, no Qt Gui.
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
   `PoseManager::Rotate(piece, degrees)`.
2. The manager reads the current project from `HistoryManager`, builds
   the next project value, and hands it to `HistoryManager::Commit` with
   a label such as "Rotate head".
3. `HistoryManager` stores the old value for undo and emits `Changed`.
4. Widgets repaint from `HistoryManager::current()`.

Nothing else writes to the project. That gives undo, redo, dirty state
and repaint for every feature without each feature writing them again.

A drag is one undo step: the tool opens an `EditScope`, calls
`Preview` on every mouse move, and the scope commits once when it goes
out of scope. `Cancel` (Escape) puts the start back.

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
  length, it does not force them.
- Playback is driven by the audio clock, so picture never drifts from
  the song.

## Dolls

A doll is a folder in the doll library:

- `art.json` and the PNGs come from Krita. Snapper only reads them.
- `rig.json` belongs to Snapper: parents, pivots, draw order, IK chains,
  warp grids, default drawing per piece.

Because Krita and Snapper write different files, re-exporting from
Krita never touches the rig. When Snapper loads a doll it matches rig
pieces to art pieces by name: new art pieces get a default rig, rig
pieces whose art is gone are flagged for the user, nothing is silently
dropped.

In Krita, a group is a piece and each child layer is one of its
drawings (head: happy, angry...). The visible child is the default. A
plain layer is a piece with one drawing.

A piece can:

- rotate, move, scale (separately in x and y) and skew,
- swap between its drawings,
- warp locally through a grid of handles, so one-piece parts such as a
  trunk can still crease at the collar or chest,
- belong to a two-bone IK chain. IK is a posing tool: dragging a hand
  solves the chain and writes ordinary rotation keys. Playback only
  ever plays keys, so what was posed is what plays.

## Shots

A project is one song and an ordered list of shots on a master track.
Each shot has its own stage:

- background (colour or image), dolls (actors), props (single images),
- text layers for lyrics,
- a camera (pan, zoom, rotate, shake),
- effect clips (flash, fill, invert, zoom punch, halftone, glitch),
- a transition into the next shot (cut, swipe, flash).

Every animated value is a channel of keys, and every channel uses the
same key type and the same sampling code, whether it moves an arm, the
camera, or a glitch amount.

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
| `SelectionManager`     | What is picked: pieces, keys, shots            |
| `PoseManager`          | Pose edits, IK drags, copy, paste, mirror      |
| `KeyManager`           | Keys on the timeline: move, hold, ease, retime |
| `PresetManager`        | Motion presets and saved poses                 |
| `PlaybackManager`      | Playhead, play, pause, loop, audio clock       |
| `ExportManager`        | MP4 and GIF export on a worker thread          |

A manager that grows past one concern is split, not grown; a file stops
at 400 lines.

## Errors

No exceptions. A call that can fail returns `Result<T>`
(`std::expected<T, Error>`), and the caller handles it. An error the user
caused is shown to the user in plain words. A broken invariant is an
`assert`.

## Limits

Every collection has a fixed maximum (`kMax...` next to its type), and
every loop is bounded by one. Ids are never reused within a project.

## Tests

- Each module has its own test program under `tests/<module>/`.
- `anim` and `io` are tested as pure functions.
- Managers are tested headless, through their public methods.
- `render` is tested by rendering offscreen and checking pixels.
- `media` is tested by encoding a clip and decoding it back.
- CTest also runs the Snawy's Law linter and the layer check.

## Folder layout

```
src/<module>/        code, one manager or concept per .h/.cpp pair
tests/<module>/      that module's tests
tools/               build checks (layer check)
cmake/               shared CMake functions
```
