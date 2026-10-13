# Snapper architecture

Snapper is a puppet animator for music videos in the style of Execution
Clap. Dolls are drawn in Krita, rigged and posed in Snapper, cut into
shots, dressed with camera moves, text and effects, laid out with video
files (a speedpaint) on the reel, and exported as MP4 or GIF, or with alpha as PNG frames, WebM or MOV. Everything is finished inside Snapper.

This file is the map. Each section is a rule the code keeps. When code
and this file disagree, one of them is a bug.
`docs/feature-map.md` says which section a change needs.

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
   A `HistoryManager::BurstGuard` held around an edit (the stage wheel)
   folds it into the last step when it has the same label and comes
   within `kBurstMs`, so a turn of many ticks is one undo.

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

Each step also remembers what it changed (`StepScope`, `edit/step_scope.h`,
pure): which shots, which layers' own warp dot settings (counted with
the shot the layer is in), or something shared (shots added or moved,
the cast, the reel, the song, the canvas, the name). `UndoIn(shot)` and
`RedoIn(shot)` act on the latest step of that shot only, taking it out
of the middle of the history when other shots were edited since
(`WithScopeFrom` copies just that shot back; the snapshots of the steps
newer than it, and the states waiting to be redone, take the shot as it
now is, so a later plain undo never brings it back). A step that
changed several shots, or the shared things, comes off only when it is
the newest. The window uses these in the Pose and Rig tabs (the shot
being worked in) and plain `Undo` in the Video tab; the Undo and Redo
buttons and menu entries name and grey out per shot.

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
  timing stays. The 1 to 4 keys do it from anywhere in the window
  (`SpaceKeysShortcuts`); a toast gives the reason when they cannot.
- A timing chart sets the spacing of the in-betweens between two
  frames A and B without moving any frame or key in time. The math is
  pure (`anim/timing_chart.h`): presets to fractions, tick drag
  clamping, `TickFrames` (which frames get ticks) and `BakeChannel`,
  which writes a key on each tick frame at its fraction of the way
  from the pose A has to the pose B has. The chart's Keys: choice
  (`KeyMode`, remembered in settings) is Existing keys only (ticks are
  the keys already between A and B, only their poses change; a key is
  added only on A or B if missing), or Add on 1s, 2s or 3s (a key
  every 1st, 2nd or 3rd frame
  from A, B always keyed; in-between keys off that grid are removed).
  Frames outside A..B never change in any mode.
- The timeline's corner is a two-icon switch (`StepModeSwitch`) between
  scrub mode, a film frame (the arrow keys move one frame), and
  animation mode, a diamond (they jump to the next frame where the
  picture changes: a key, a shot's start, or each frame of an ease;
  `PoseChanges`).
- A chip bar (`KeyChipBar`) floats over the picked keys on the
  timeline: Step, Ease, Line (pressed when all picked keys share it,
  `KeyManager::EaseOf`), On 2s, On 3s, and -1 f and +1 f (`KeyManager::Hold`:
  frames go in or out right after the last picked key and later keys
  slide). Each makes the same call as the right-click menu and is
  greyed with its reason; `KeyChipBar::SpotFor` is the pure placement.
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
plain layer is a piece with one drawing. The exporter also writes each
drawing's layer name and the folder groups around each piece
(`ArtPiece::layer_names`, `folder`); `DrawingName` gives a drawing's
name, "head 1" for dolls exported before names were kept.

Doll to Krita goes the other way. `io/ora_file.cpp` writes a doll as
an OpenRaster file (`io/zip_writer.cpp` makes the zip) with the groups,
names, the default drawing shown and a hidden "Snapper rig" layer of
joint crosses; `DollLibraryManager::ExportToKrita` writes it and keeps
its path in the library rig.json (`Rig::krita_file`), and
`ui/krita_menu.cpp` asks, writes and opens Krita. The Krita plugin's
Import Snapper Doll does the same inside Krita. A reload names the
drawings a re-export lost (`RemovedDrawings`).

The floating palette (`PaletteManager`) gives every feature that asks
for a color the same colors: the picked piece's and doll's solid
colors (`render/drawing_colors.cpp`, cached per drawing file), the
project's recent ones (`Project::recent_colors`, recorded by the edit
that uses a color through `WithRecent`) and the doll's hand-added ones
(`Rig::added_colors`). It never changes a drawing. It also holds the
current and hovered color, so `ui/palette_panel.cpp` and the stage
(flash, I to pick) agree; draw mode opens and closes it with `SetShown`.


A doll is keyed as a whole, as a drawing is in traditional animation:
posing any piece keys every piece and the layer's own move at that
frame, so one key on the doll's timeline row is its full pose.

A piece can:

- rotate, move, scale (separately in x and y) and skew,
- swap between its drawings,
- warp locally through a shape mesh, so one-piece parts such as a
  trunk can still crease at the collar or chest. Ticking Bend in the
  rig makes the mesh from the piece's drawings (all of them, so any
  swap is covered): every outline of what is visible (alpha 8 and up,
  holes and separate shapes included) is grown a pixel or two and
  straightened, dots go along it and on an even lattice inside, and
  Delaunay triangles join them, outline sides kept as mesh sides
  (`BuildShapeMesh`, `anim/shape_mesh.h`; the outline tracing is the
  collider's, `anim/mask_rings.h`). Every visible pixel is inside a
  triangle and nothing is meshed where nothing is drawn. Detail (Few,
  Normal, Many) sets how many dots; the same drawings and detail always
  give the same mesh. The model keeps the mesh (`WarpMesh`, inside
  `WarpGrid`, shared between snapshots) and works out its sides, its
  average side (one cell of reach) and how far apart each two dots are
  walking along the sides, so a pull never jumps a gap in the drawing.
  Everything built on dots asks `anim/warp_shape.h` what a warp is made
  of (`WarpEdges`, `WarpTriangles`, `CellSize`, `CarriedAt`,
  `MirroredPoints`), so drawing, folding, cloth, collider pushes, point
  motions and mirroring work the same on a mesh and on the older
  rectangular grid, which old files keep and which renders exactly as
  it always did; its fields and Convert to shape mesh show only for it.
  Making a mesh again (a new Detail, a converted grid, or art redrawn
  in Krita and reloaded, `WithFreshMeshes`) is one undo step that
  carries each dot's settings to the nearest new dot within a cell
  (cloth to every new dot in its patch) and every warp key by position
  (`CarryDots`, `CarryOffsets`, `anim/mesh_carry.h`; `edit/mesh_edits.h`);
  a toast says how many dots' settings carried over.
  On the stage a picked piece shows its mesh as faint lines with
  yellow dots, where the drawing has them at that frame (keys, drag
  and physics nodes, point motions and collider pushes all in,
  `PieceWarpOnScreen`); where something moved a dot off its key, a
  faint dotted ring tied to it shows the key. Dragging a dot keys that
  point's push, moving the key by the drag. Each dot has a
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
  A drag node has a kind (`NodeKind`, set on many dots by
  `RigManager::SetNodeKindAll`): Drag (the spring above), Cloth or
  Jelly. Cloth is secondary motion for cloth and hair. A lone cloth
  dot is a heavy, jiggly point; cloth dots next to each other on the grid are tied like cloth and
  hang from the plain dots around them (pins the body carries). Its
  five settings each go from none of it (0) to a lot (1): Stretch (how
  far it may get from the dots around it: 0 keeps its drawn distance,
  1 a cell more; ties also pull back softly, less as Stretch goes
  up), Inertia (how much of its own motion it keeps when the doll
  speeds up, stops or turns: 0 moves with the doll), Air resistance
  (how fast that dies down), Gravity (a pull down in shot space by how
  far the drawing has turned from upright, sized by the grid cell: an
  upright drawing stays as drawn, a doll that falls over has its skirt
  and hair hang down) and Bounce (a spring back to the drawn shape).
  Ties never let cloth fold flat, and a grid that would turn over is
  eased back (`Unfold`, `anim/grid_fold.h`). On a mesh, cloth is also
  tied across each two triangles and a triangle it turns over is
  turned back by mirroring its node across the facing side. The math is pure in
  `anim/cloth.h` (`StepCloth`); `DraggedPoses` runs it on the same
  beat and in the same `DragTrail` as plain drag nodes, so a late
  frame costs like an early one. `doll_beat.h` holds the beat; a long
  hold still beats at least every 4 frames, so what hangs settles
  through it. Moving each node by its own motion, pushing it out of
  colliders, turning back what turned over and its speed for the next
  frame are shared by cloth and jelly (`anim/cloth_parts.h`).
  Jelly is soft flesh, a chest (`anim/jelly.h`, `StepJelly`): a
  piece's jelly dots wobble as one smooth blob. Dots joined by sides of
  the mesh are one clump; a clump's swing (how far its dots are from
  where the body carries them) is not kept per dot but as one shift plus
  one squash, stretch and turn (`anim/jelly_fit.h`, `Swing`), found by
  a weighted least-squares fit over its dots each frame, so no two
  neighbours can ever disagree. The shift and the squash each spring back
  with the cloth move (kept speed, pull); the squash springs back harder
  at low squish, may bend less (the swing is bent smoothly toward a limit
  that keeps every side above 0.4 of its length, so triangles never turn
  over) and keeps most of its area. Each dot then takes a share of the
  swing, 0 next to a pin (a grid point with no dot) and growing smoothly
  over three sides, so what is still stays still; a side may not squeeze
  below half its drawn length (a net for a hard collider), and a swing
  bends toward one blob radius and never passes it. Nothing mirrors or
  snaps a turned-over triangle back. Colliders work
  as for cloth. Jelly has four knobs, each 0 to 1 and spread evenly by
  feel, kept in DragNode's spring, inertia, stretch and gravity (no
  air): Firmness (the wobble period, about 24 frames at 0 to 4 at 1,
  the same ratio each step), Wobble (visible jiggles before it settles,
  0 to 6 in even steps), Squish (how freely the blob squashes and stretches, how far it may bend;
  even steps in how far it shows, and even at 1 some pull back to its
  own shape stays) and Droop (how far it sags when the doll tilts, the
  same at any firmness). `model/node_physics.h` turns them into the
  per-frame speed kept, pull, squish and fall (`JellyMotionOf`, a
  damped spring with that period and damping), and turns a jelly node
  saved before the knobs, in the cloth numbers, into the knobs that
  move nearest to it (`JellyFromOld`, used by `io/doll_json.cpp`; new
  files save firmness, wobble, squish and droop). `ClothSettingsOf`
  gives both kinds their per-frame numbers. A piece's cloth and its
  jelly run apart, each seeing the other's dots as pins.
  A node's feel (`anim/node_feel.h`) is two numbers, firm and heavy,
  that set its kind's settings (`WithFeel`, read back by `FeelOf`); a
  preset (Pudding, Silk, Snappy...) is a named feel.
  `RigManager::SetFeelAll` sets it on many dots, each by its own kind
  (`edit/rig_nodes.cpp`, beside `SetNodeKindAll`).
  A piece can be a collider (right-click it in the rig: Make
  collider): the outline of its visible pixels, traced into a ring of
  dots about 12 px apart (`TraceOutline`, pure, in `anim`; at most
  `kMaxColliderDots`), kept in the piece's drawing pixels so it follows
  the pose, the warp grid (`ShotPoints`) and links. A collider belongs
  to one drawing of the piece: `RigPiece::drawing_colliders` keys them
  by drawing name (`DrawingName`, so a re-export in a new order keeps
  them, and `Reconcile` carries them over), and a drawing without its
  own uses the piece's `collider` (old rigs load unchanged; an empty
  one in the map means that drawing has none; `DrawingCollider`). The
  rig canvas and `ColliderBox` make, show and edit the collider of the
  drawing the piece shows in the Rig tab (the rig's default, or the
  skin's while one is being changed: `RestDrawingName`), through
  `RigManager`'s collider edits with that drawing's name, one undo step
  each; the physics uses the collider of the drawing each piece shows
  at that frame (`ShownCollider`). rig.json keeps them under
  `drawing_colliders`. Dots can be
  deleted in the rig; an edge joins only dots that were neighbours on
  the ring, so a gap pushes nothing. A collider pushes every warp dot
  of the doll's other pieces, in front or behind (two hands with colliders
  push each other apart), plain, drag or physics alike, except the pieces it spares (ticks in
  the rig panel's `ColliderBox`, greyed with the reason for a piece
  with no grid; `RigManager::SetColliderPushes`). What is pushed is
  measured against how the two pieces were drawn: each dot has a
  ghost, where it would be had it kept its drawn place against the
  collider (`PushFieldsFor`), and walking from the ghost to the dot,
  the last edge crossed tells whether it came in (`PushedFromRest`).
  So a skirt drawn over a thigh is left as drawn, and only what the
  thigh comes into since is pushed: straight back out across the run
  of outline it came in through, on the side it came from, never
  held or pulled, so the skirt is free to fly up off the leg. Every
  drawn edge between two dots is pushed out the same way (seven spots
  along it), each push spreads to the neighbours by the dot's rubber
  reach (at least `MinPushSpread`: on a mesh as far as on a 3 by 3
  grid) so the drawing bends instead
  of kinking, and a push that would fold the drawing gives way
  (`PushGrid`, `anim/grid_push.h`). Plain dots are pushed per frame
  from where the keys put them, with no history (`CollidePoses`, in
  `DraggedPoses` before the trails), back to front so a pushed piece
  pushes from where it was pushed to. Two pieces whose colliders push
  each other (`PushField::is_shared`) share the move: the one handled
  first takes half (`kSharedShare`, `PushGridShared`) and the later one
  pushes the rest, so both squash alike instead of one giving way for
  both. Drag and physics nodes hang from
  the pushed dots and swing on top of the push; physics nodes a
  collider comes into are also pushed inside their simulation, back
  toward where they belong (`OutToward`), keeping a little of the hit
  as speed. With no collider nothing changes. The dots show (teal,
  small look big grab) on the rig canvas only while their piece is
  picked: click picks one, Shift adds, Delete removes the picked ones
  as one undo step, Escape lets go. They are never drawn on the stage.
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
- mirror their dot setup to the other side: right-click a piece in the
  rig (or the rig panel's button) > Mirror rig to other side copies its
  drag and physics nodes, its point motions (directions flipped) and its
  colliders, per drawing too (each onto the twin's drawing named with
  the side word swapped, else the same name, else in the same place),
  with which pieces they push,
  onto its left or right twin, mirrored, replacing the twin's
  (`RigManager::MirrorNodes`, twins found by `MirrorName`, the same
  pairing Copy to other side uses). The Drag node panel's Mirror to
  other side does it for just the picked dots (`MirrorDots`). Dots pair
  by place: an old grid with the same grid dot for dot, a mesh with the
  twin's dot at the mirrored spot (`MirroredPoints`); Copy to other side
  gives a meshed twin its own mesh from its own drawing. Greyed with
  the reason when the piece has no twin, either does not bend, or two
  old grids differ.
- belong to a two-bone IK chain. IK is a posing tool: dragging a hand
  solves the chain and writes ordinary rotation keys. Playback only
  ever plays keys, so what was posed is what plays.

Warp dot settings (rubber reach, drag and physics nodes, point motions)
belong to a layer of a shot, not to the doll, so changing the jelly of
a doll in one shot leaves the same doll in another shot alone. A rig
piece holds the doll's default in its own fields (what a new layer
starts with) and, in `layer_dots`, the settings of each layer that has
changed them for itself (`PieceDots`, by `LayerId`). `DotsOf`,
`PieceFor` and `DollFor` (`model/doll.h`) give a layer's view; the
poses, springs, stage geometry, link solver, poke check and the panels
read that view. A `DotRef` with a layer edits that layer's copy
(`WithDots`); one without edits the default (the Rig tab). A change to a
piece's grid clears every layer's copy of it, a mesh carry moves them
along with the default, and copying a layer or a shot copies them
(`WithCopiedLayerDots`). Set as default (`RigManager::SetDotsDefault`)
makes a layer's settings the doll's default after giving every other
layer of the doll the look it has now, so only layers added later start
from it.

Smart dials (`model/smart_dial.h`, `DialManager`, `anim/smart_dial.cpp`)
are named controls in the doll's rig (saved in rig.json) that move a
set of pieces between up to 8 stops on a 0 to 1 track. Each stop holds
a pose per driven piece and a drawing by its Krita layer name; between
stops `DialPoses` blends the poses and swaps the drawing halfway or at
the stop. The value is keyed per doll layer (`DollLayer::dials`, track
kind `kDial`), on the doll's own timeline row, and keying any part of a
doll keys every dial too. `SamplePoses(doll, layer, frame)` puts the
dials under the hand pose (turn, move, skew and order add; scale and
opacity multiply; a hand-picked drawing wins), so physics, IK and links
read the result. Set stop takes the hand pose of the driven pieces at
the playhead into the stop under the knob and resets those hand keys,
so the doll looks the same after. `DialBox` (`ui/dial_box.cpp`, the
Smart dials fold of the inspector) makes dials from template tiles and
turns them (`DialTrack`: snap to stops, Ctrl free, Shift fine, double
click to rest).

Skins (`model/skin.h`, `SkinManager`, `anim/skin.cpp`) are named looks
in the doll's rig (`Rig::skins`, rig.json, kept by `Reconcile`): piece
to drawing by Krita layer name, an empty name hiding the piece. Which
skins a layer wears is its own keyed track (`DollLayer::skins`, a
`Channel<SkinSet>`, track kind `kSkin`, on the doll's timeline row), not
part of the whole-doll key, so a later pose key never takes an outfit
off. `SamplePoses(doll, layer, frame)` runs `ApplySkins` after the
dials: a hand-picked drawing beats every skin, a dial's drawing turns
into the worn skin's "skin/drawing" version where the doll has one,
otherwise the first worn skin in the list that names the piece sets
its drawing or hides it (opacity 0). `SkinRigBox` (Rig tab) makes,
renames, orders and deletes skins and picks one to change: the rig
canvas wears it (`WornFromStart`) and the Drawing picker writes into
it. `SkinBox` (the inspector's Skins fold) wears them at the playhead;
Shift+[ and Shift+] step through them.

Draw on a piece (`DrawManager`, `edit/draw_manager.cpp`,
`edit/draw_save.cpp`) draws a new drawing for a piece inside Snapper.
The session (the drawing being made, at the piece's size, the tool,
brush size and stroke undo) lives outside the project's history; the
hard pixel brush, line and fill are `render/pixel_paint.h`. Done saves
a PNG in the doll folder's `snapper` folder and lists it in the rig
(`RigPiece::own_drawings`, rig.json), since art.json is Krita's;
`AddOwnDrawings` (`io/doll_file.h`) lists those after the piece's
Krita drawings in the doll's art whenever a doll is loaded, reloaded
or read from a project, so keys, the Drawing picker and dial stops see
one list. Done is one undo step that also shows the new drawing on the
piece at the frame drawing began; a new version of a Snapper drawing
gets a new file, so caches never show a stale one. The Krita exporter
carries the `snapper` folder over like rig.json. `DrawCanvas`
(`ui/draw_canvas.cpp`) covers the stage while drawing: the piece at its
rest place over the dimmed doll at rest (`RestLayer`, shared with the
rig canvas), with `DrawBar` (title, name, Done, Cancel) and
`DrawTools`; N, Shift+N, the stage's right click and the Drawing
picker's "+" entries start it (`ui/draw_actions.cpp`).

Objects (`ObjectManager`, `edit/object_manager.cpp`) are props made
from a PNG, GIF or WebP: a doll with one piece whose drawings are the
picture's frames, marked by `Rig::object` (`ObjectClip`: the flag and
each frame's own milliseconds, rig.json, kept by `Reconcile`), so warp,
jelly, skins and drawing on a piece work on it unchanged.
`io/object_file.cpp` reads the frames (more than 64 keep every Nth)
and writes the doll folder into the doll library: "frame N.png",
art.json (`WriteArt`, the one art.json Snapper writes) and rig.json;
it is copied into a project like any doll. The Cast panel's Objects
row lists them (`ObjectManager::Objects`; the doll row shows the rest)
and takes dropped files; files dropped on the stage become objects
placed where they fell, one undo step. A layer's `DollLayer::play`
(`ObjectPlay`: Loop, Once, Ping-pong or Hold, frames per drawing or
the picture's timing, start, offset) is played on the shot clock by
`ApplyObjectPlay` (`anim/object_play.cpp`) at the end of
`SamplePoses`, so scrubbing and export agree; Hold keeps the keyed
drawing. `ObjectBox` (inspector) sets it. Holding is in Links below.

## Poking

Holding P over a doll on the stage and dragging or flicking pokes it
like a toy: its drag, cloth and jelly dots wobble and settle. Nothing
is keyed and nothing goes into history; the project never changes.
`PokeManager` (edit) owns the live session. `PokeLayer` (anim) makes a
copy of the doll's layer that holds its pose at the poked frame and is
shoved by a share of the mouse move (`PokeShove`, small and capped) on
each virtual frame after it; `ShownPoses` on that copy at frame + k
gives the wobble, through the manager's own `DragTrail`, so each tick
is cheap. A timer ticks one virtual frame at the film rate while the
poke lives. It ends by itself when the dots have been calm for a few
frames after the mouse is let go, after six seconds, when the playhead
moves or the doll is gone; Escape or a click ends it at once. An edit
(a setting tuned in a panel) runs the same poke again on the edited
doll. The stage hands the poked poses to the shared `PoseMemo` as an
override for that layer (`SetOverride`), which `LinkSolver::Poses`
returns first, so painting, hover and handles all see the wobble.
`StagePoke` (ui) is the stage's side: P arms it (open hand over a
pokeable doll, closed hand while poking, a toast saying why when the
spot or doll cannot be poked), a press starts the poke, the mouse
shoves it, release lets it settle. P is free of other shortcuts.

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

A shot may hold references (`Shot::references`, `model/reference.h`):
videos, GIFs and pictures linked by path to animate over. They are not
layers and have no keys, so posing, key copy and paste never touch
them; `ReferenceManager` owns every edit (one named undo step each) and
the view state (the picked one, Y hiding them all), and hands out each
one's picture of the moment through its own `VideoFileFrames`. Only
`StageView` draws them (`ui/stage_view_reference.cpp`): the ones behind
go to `FrameRenderer::RenderFrame` as its `under` picture, drawn over
the background and below every layer; the ones in front are painted
over the frame. Nothing else passes `under`, so export, the Video tab,
thumbnails and the reel never show a reference. They are placed in
canvas space, without the camera. The Pose timeline gives each its own
row above the doll rows (`ui/timeline_references.cpp`), not counted in
the shot's length.

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

An object can be held by a doll piece (`Hold` in `Shot::holds`, made by
`ObjectManager` from the stage's right click, `ui/hold_actions.cpp`).
Unlike a link it copies the piece's full move, place, turn and scale:
`LinkSolver::HeldMove` walks the object's hold changes up to the frame
and, for the one in force, gives the move as it was when the hold began
times the inverse of the holder's transform then times the holder's
transform now, so nothing jumps at a change; a drop (no holder) keeps
the move it had. It sits between the layer's own keys and its link, so
its keys still tilt it in the hand. Every place that asks the solver
for a layer's transform sees it; the pose tool drags a held layer in
its held space (`LayerParentToScreen`). A hold whose holder layer goes
becomes a drop (`DropDeadLinks`); holds loop-check in the manager and
the solver treats a loop as not held.

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
and frame 0 (`SnapDelta`, `SnapFrame`); Ctrl turns snapping off.

A shot clip showing its whole shot (a full clip) resizes the shot
when an edge is dragged; Alt trims instead, and on a partial clip Alt
resizes. `ReelManager::ResizeShots` (`edit/reel_resize.cpp`) works from
the drag's start: the shots of the grabbed and picked shot clips get
the same frames more or fewer, by the edge mode (Hold, or Stretch with
`StretchShot`); a start edge adds frames at the front and moves every
key, link, hold and layer span (`ShiftShotFront`, `edit/shot_timing.h`).
Every full clip of a resized shot follows, dragged edges move, partial
clips keep their in and length, Shift ripples later clips, and a clip
in the way stops the drag at the most that fits. Keys past a shot's end
are kept, hidden (`HiddenKeyFrames`, a badge on the clip).
`FitShotToGap` lays a shot clip over the gap between two markers with
its shot exactly as long.

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

Sound lives only on the reel, on sound tracks under the picture ones
(`ReelTrack::is_sound`, at the bottom of `Reel::tracks`; 2 by default,
up to `kMaxSoundTracks`). A sound clip is a `Clip` whose source is a
`SoundSource` (a file by path), with a volume and fades; a sound track
has a name, Mute, Solo and a volume. Sound clips move, trim, split,
copy, paste and snap through the same edits as picture clips, and
`WrongTrack` keeps each kind on its own tracks. Sound has no picture
(`ReelAt` skips sound tracks) and does not set the video's length
(`ReelLength`; `SoundEnd` for the sound). A file from before sound
tracks, or a new project, gets the default sound tracks with the song
laid from frame 0 on the first (`ReelManager::AddSoundTracks`, called by
`DocumentManager`); choosing a song replaces its clip (`ReplaceSong`).
The sound edits are in `edit/reel_sound.cpp`. `SoundManager` decodes
each file once in the background, keeps its waveform, and mixes the
heard tracks (`MixReel`). Shots have no sound of their own: on the
shots timeline a shot placed on the reel plays the reel's mix under its
clip (the one last picked on the Video tab, else the first;
`SoundClipOf`), silent where the clip leaves its frames out, and any
other shot plays the song where it sits (`MixShots`).
`PlaybackManager::SoundOn` is what each timeline plays, and export
mixes the same (`ExportManager::ExportSound` writes it as a WAV).

Text lives on the reel too. A text clip is a `Clip` whose source is a
`TextSource`: its words, a `ReelTextLook` (a shot text layer's
lettering, a box color, and its middle as a share of the frame, so a
new canvas size keeps it in place), how it appears and leaves
(`TextMotion`, played inside the clip's ends) and the named style it
follows. It has no length of its own (`SourceLength` is endless, its
`in` stays 0) and moves, trims, splits, copies and snaps through the
same edits as other clips. The one subtitle track (`ReelTrack::
is_subtitle`, last in `Reel::tracks`, so drawn on top; made by the
first cue) holds only text clips, its cues; a cue shows the reel's
`subtitle_look` unless it keeps its own (`LookOf`). `Reel` also keeps
the saved `text_styles` and the `subtitle_lines` waiting to be timed;
they save under `reel_text`. ReelManager owns every text edit
(`edit/reel_text.cpp`: add, words, look, drag, place tiles, motions,
styles; `edit/reel_subtitles.cpp`: tap L to time a line (`TapCue`,
`ReleaseCue`), roll a cue edge with its neighbour, SRT and WebVTT import
through `io/subtitle_file.h`, find and replace, `CuesOf`).
`PaintReelText` (`render/reel_text.cpp`) draws a text clip in
`RenderReel` through the same `TextPath` and `PaintLettering` as a text
layer, wrapping lines at the title-safe width (`kTitleSafe`). Export
burns the cues in, writes them beside the video (`SubtitleOut`,
`ExportManager::WriteSubtitles`: SRT, WebVTT beside a WebM) or both.

## Rendering

`FrameRenderer` draws on the CPU with `QPainter` into a `QImage`, so it
runs the same in the window, in an export thread and in a headless
test. Warped pieces are drawn by `WarpImage`, which maps each triangle
(a mesh's own, or each grid cell as two) back into the drawing. Each renderer has its own
`ImageCache`; nothing is shared across threads. The cache also holds a
`WarpCache` of bent drawings (`ImageCache::Warped`), so painting and
hover hit tests bend a drawing once per pose, not once per paint. It also owns a
`PoseMemo` (`anim/pose_memo.h`): the poses `LinkSolver` solved for the shot
being drawn, by layer and frame, so paint, hit tests and handle overlays
solve a frame once, and keeps each doll layer's `DragTrail` (where the
drag-node springs stood after each frame) so a later frame carries on
instead of running the springs from frame 0. It holds one shot by identity and empties itself when
an edit makes new shot or doll pointers. Preview may render at a
smaller scale; effects scale their sizes with it so they look the same.

The stage is a pasteboard: `RenderArea` draws every layer of a shot but
effects over any area, reaching past the canvas, with the camera but
not its shake. `StageView` (`ui/stage_view_outside.cpp`) draws it at
40 % behind the frame picture, darkens the stage outside the frame a
little and draws a thin frame line; O hides it, remembered per project
file by `DocumentManager` (in the app settings, not the project, not
undoable). Hit tests were never bound to the frame, so picking,
handles and drags work outside it unchanged. Shift+3 fits all the shot
holds; a red arrow at the view edge points to a picked layer out of
view. Export still renders only the frame.

Each doll's rig has a look (`DrawingLook`: Crisp, Smooth, Smooth
flat) and a Hard edges switch; a piece may stay Crisp. `LayerPainter`
turns them into a `WarpLook`. Crisp reads the nearest pixel, as
before. Smooth draws from a copy upscaled 2x or 4x by Scale2x
(`render/upscale.cpp`, 2x only past 2048 pixels a side), kept in the
`WarpCache` within its byte budget: warps blend between its pixels
(bilinear) and may be bent up to twice the drawing's size when zoomed;
turned or scaled pieces draw the copy with the painter's smooth
scaling over the same rectangle, so nothing moves. Smooth flat keeps
fully covered pixels on the drawing's own colors; Hard edges keeps or
drops edge pixels whole. Hit tests ask for the default look, so they
read the original drawing. Holding backslash on the stage sets the
stage renderers' `ImageCache` to Crisp only. RigManager owns the
edits; a rig without a look opens Crisp, a new doll starts Smooth
(`ReadRig`).

`RenderReel` draws a reel frame: shot clips through `RenderShot`, video
clips fitted to the canvas, keeping their shape. Over a transition both
clips are drawn over the tracks below and mixed by `Combine`. Pictures
of video files come through the `VideoFrames` hook, so drawing never
opens a file; the `edit` side's `VideoFileFrames` reads them with
`VideoReader`, keeping each file open so playing forward decodes each
picture once. Like the renderer, each thread has its own.

A shot may be see-through (`Shot::is_transparent`; `background` stays
remembered). `RenderShot` then starts from a clear frame, and effects
other than zoom punch are cut back to what was drawn before them
(`KeepDrawnOnly`), so a fill or flash never covers the empty area. In
the reel a see-through clip shows the tracks below; `RenderReel` takes
what lies under every track (black, or clear for an alpha export).
`ShotManager::SetTransparentAll` is the one edit (one undo step for all
picked shots); `Project::new_shots_transparent` (the Transparent start
in the new project dialog) makes new shots start see-through. The
stage paints a checkerboard behind such a shot (`ui/checkerboard.h`,
also the strip and reel badges); Shift+V held shows it over the
palette's current color instead.

Export formats live in `media/video_formats.h` (`VideoFormat`,
`KeepsAlpha`, `HasSound`, `WhyNoEncoder`, `ReadyFor`). `VideoEncoder`
writes MP4, GIF, WebM (VP9 `yuva420p`, Opus or Vorbis), MOV (ProRes
4444 `yuva444p10le`, PCM sound) and PNG frames (`media/png_frames.cpp`:
a folder of `frame_00000.png`, the song as `sound.wav` via
`media/wav_file.h`). Each frame goes through `ReadyFor`: MP4 lays a
see-through shot over its remembered color (the export worker passes
it per frame), GIF blends soft edges over it and keeps one clear
palette slot, the others keep alpha. `ExportManager::WhyNoExport`
refuses a format whose encoder this FFmpeg lacks, saying which.

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
| `DocumentManager`      | New, open, save, autosave, recent files, the per-file outside switch |
| `DollLibraryManager`   | Doll folders, loading, reload on re-export     |
| `RigManager`           | Rig edits: joints, IK, shape meshes and warp grids, their dots, drag and physics nodes, colliders |
| `PokeManager`          | Live poke of a doll on the stage: never in history (see Poking) |
| `ShotManager`          | Shots on the master track: order, length, look |
| `ReelManager`          | The reel: clips, tracks, cuts, transitions, sound clips, volume, fades, Mute and Solo |
| `SoundManager`         | The reel's sound files decoded and their waveforms; the mix; which clip a shot borrows its sound from |
| `StageManager`         | What is in a shot: actors, props, text, effects |
| `LinkManager`          | Which things copy another's movement, how much |
| `SelectionManager`     | What is picked: shots, layers, pieces, keys, clips |
| `PoseManager`          | Pose edits, IK drags, copy, paste, mirror      |
| `DialManager`          | Smart dials: make, stops, rest, swap, turn (keyed per doll layer) |
| `SkinManager`          | Skins: make, rename, order, set drawings, Krita names; wear and step them (keyed per doll layer) |
| `DrawManager`          | Draw mode: the drawing being made, its tools and stroke undo; Done saves it into the doll as one step |
| `ObjectManager`        | Objects: make them from pictures into the library, put them on the stage, how a GIF plays, hold, drop and hand-off |
| `KeyManager`           | Keys on the timeline: move, space, ease, retime, bake a timing chart |
| `TimingChartManager`   | The timing chart session: picked thing and frames to chart, ticks, live preview in one `EditScope`, Apply or Cancel |
| `PresetManager`        | Saved poses                                    |
| `PlaybackManager`      | Playhead on either timeline, play, loop, song, the sound each timeline plays |
| `ExportManager`        | MP4, GIF, PNG frames, WebM, MOV export, threaded; sound only as WAV |
| `MotionManager` (`ui`) | How long and how softly the UI moves: the Motion table, Full / Calm / Off, saved in settings |
| `ToastManager` (`ui`)  | Toasts: the queue, their time, stacking, dismissal and the Undo they offer |
| `TimingChartDialog` (`ui`) | The floating timing chart window (Shift+T, or Define timing chart... in the stage and timeline menus); its strip only draws the ticks and forwards drags to `TimingChartManager` |
| `SpaceKeysShortcuts` (`ui`) | The 1 to 4 window shortcuts and the Space evenly menu; both call `KeyManager::Space` through `RequestSpaceKeys` |
| `DragPlayhead` (`ui`) | The playhead's place before a key or clip drag: the drag carries the playhead, Escape puts it back, no history |
| `PaintClip` (`ui`) | RAII clip so timeline keys, bars, grid and playhead paint only in the frame area, never over the names |
| `RefusalShaker` (`ui`) | The shake and the reason on a greyed control clicked anyway |
| `DragBox` (`ui`) | The warp dot panel: material tiles, a golden-ratio row of feel pad (62 %) and live wobble (38 %) on raised plates over sunken frames, preset chips, a reach fader, a folded Fine tune section of faders, the dot groups (`GroupBox`, shown while the pick has dots, drag nodes, motions or groups), Mirror and Set as default |
| `SettingsManager` | Ctrl+Shift+C / V: copy everything about the picked part except its pose at the playhead (warp dots, flip, text look, effect kind and colour) and paste it as one step; the same piece of the same doll also gets its groups and drawing; `SettingsActions` (`ui`) are its menu entries |
| `StepScope` (`edit`) | What an undo step changed, for per-shot undo (see Project data and undo) |
| `KindTiles` / `KindTile` (`ui`) | The four material tiles (Still, Drag, Cloth, Jelly): a tiny cartoon each, looping only under the pointer, squashing on a press |
| `FeelPad` / `WobbleBlob` (`ui`) | The soft-firm by light-heavy pad that reports Grabbed, Moved, Released, Cancelled; and the small live preview of the first picked dot, painting a `HopPreview` |
| `HopPreview` (`anim`) | The preview blob that hops on a floor on a 1.2 s loop, its edge dots run by the stage's own jelly, cloth or drag spring step; each hop from rest |
| `FoldSection` (`ui`) | A heading that slides its rows away over the Move time; open or closed is kept in the settings file |

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

Warp dots are drawn by how they move, the choice in `dot_shape.h`
(pure): a still dot is plain, a drag node a yellow ring with a short tail
toward its keyed place, cloth a diamond, jelly a blob stretched along
its offset from the keyed place (round when Motion is Off). The stage
paints them from `WarpOnScreen::kinds`.

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
long. On the stage, `StageFlash` (`ui`, one per `StageView`, with its own
`ChangeWatcher`) shows undo and redo: `ChangedLayers`
(`edit/change_diff.h`) names the layers of the shown shot that differ,
and the stage flashes white the drawn pixels of each piece of a doll
that stands differently (the whole picture for other layers; the
frame renderer draws just those), with a faint red copy fading where
it stood, in the Move time. A fresh edit
flashes nothing; in Off there is none.
Each timeline's playhead lands on a frame with a small overshoot
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
slides under the active tab through `MotionManager`) and the Undo and
Redo buttons. The play controls are a `TransportBar`, one above the
pose timeline and one at the left of the video page's button bar (none
on the Rig tab). Both forward clicks to `PlaybackManager` and
`HistoryManager` and decide nothing: the words, tooltips and
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

Warp dots pick the same way (`SelectionManager::PickDots`): a click
on a dot picks it, Shift adds, Ctrl flips, and a box drawn from empty
space on the stage picks every dot of a picked piece inside it (Shift
adds, Ctrl takes out); a box over no dot picks pieces as before. On the
rig canvas the same box picks collider dots. The Drag node and Dot
animation panels edit every picked dot at once: a number that differs
shows Mixed, a change sets them all and is one undo step named with the
count ("Set bounce on 6 dots"; `RigManager::SetDragField`,
`SetDragNodes`, `SetNodeKindAll`, `SetMotionAll`, all taking `DotRef`s).
D switches every picked dot's drag node, and Delete clears the drag or
physics node and the motion off every picked dot (`ClearDots`).
Dot groups (`model/dot_group.h`, `GroupManager`) are named sets of warp
dots, so a set that must stay still and a set that must be jelly are
each picked with one click. A shot keeps its own groups
(`Shot::dot_groups`, dots with a layer); a default is kept on the doll
(`Rig::dot_groups`, dots with no layer) and every shot with that doll
shows it, a shot's group of the same name hiding it. `ShownGroupsOf`
(`edit/group_edits.h`) is the one view of both, with dots no longer on
the stage left out; `WithGroupWritten` is the one write path, so create,
rename, add or remove dots, make default or shot-only and delete are each
one undo step and a renamed group keeps its place. `GroupManager` holds
the rules and the words for refusals; `SelectionManager::PickGroup` picks
a group's dots and their pieces (Ctrl on a wholly picked group takes the
dots out). `GroupBox` (shown while a doll is picked) is a chip per group,
pressed when all of it is picked (only the groups with a dot on a picked
piece show, so a limb shows its own), a Group button greyed with the reason,
and a right-click menu; `group_chips.h` is its pure part. Copying a side
to the other, and Mirror to other side on picked dots or a piece
(`WithTwinGroups`, `edit/group_mirror.h`), also make a twin group of the mirrored dots for each group that holds dots of a copied
piece, so the other side has its own group: a group without a side in
its name takes its pieces' side ("hair" becomes "hair_L", the twin
"hair_R"), and copying again remakes that twin, never a new one. Both
are saved
(`io/dot_group_json.h`).
The Warp dots panel (`DragBox`) shows what the picked dots are as four
material tiles (`KindTiles`): a click makes every picked dot Still
(`SetDragNodes`) or a node of that kind (`SetNodeKindAll`), one undo
step. `dot_kinds.h` is the pure part, tested without a widget: `KindMix`
counts the materials among the picked dots (which tile is outlined, thick
for all and thin for some, and whether the title says Mixed),
`FineRowsOf` and `FineLabel` say which exact rows apply (jelly alone
shows Squish, Firmness, Wobble and Droop, and no Air resistance), and `DistinctFeels` lists the pucks. The feel pad
(`FeelPad`) only reports a drag; `DragBox` opens one `LiveEdit` burst on
Grabbed, calls `SetFeelAll` on every Moved, closes it on Released and
cancels it on Escape, so a whole drag is one undo step. Preset chips call
`SetFeelAll` with `FeelPresets`. The tiles and the wobble preview animate
only through `MotionManager`: nothing moves in Off.
The wobble preview (`WobbleBlob`) only paints `anim/hop_preview.h`: a
small round mesh (a pinned core, a ring of edge dots) whose body hops
on a fixed path (`HopAt`: lift off, a slight lean so Droop shows, land)
while the edge dots run the stage's own step for the first picked
dot's settings: `StepJelly`, `StepCloth` with the floor as a collider,
or the drag spring (`anim/drag_spring.h`, shared with `DraggedPoses`).
Each hop starts from rest: hopping in time with a soft, wobbly blob
would otherwise swing it higher every loop. A new feel of the same
kind carries on mid-hop, so dragging the pad does not restart it.

The stage view looks at the frame through a camera (`StageCamera`,
`render/stage_camera.h`, pure): Ctrl+wheel zooms toward the cursor, the
middle button drags the view, Shift+1 fits the frame, Shift+2 frames the
pick and Shift+0 is actual size; the zoom shows beside the cursor. The
tools take their frame (scale and corner) from the camera, so picking
and dragging work at any zoom. The arrow keys nudge what is picked 1
pixel (10 with Shift) as one undo step (`PoseTool::Nudge`) while the
stage has focus. With keys picked on the timeline, a move, a scale, a
turn by the wheel or a nudge lands on every picked frame of the shot, each
getting the same amount added to what it has (`PoseTool::EditFrames`,
`PoseManager::Shift` over many frames, still one undo step); with none
picked it lands on the playhead's frame as before. Tab and Shift+Tab step the pick through the pieces of
its doll or the layers (`SelectionManager::StepPick`). A move drag
catches on guides (`SnapToGuides`, `anim/guides.h`, pure): its box's
edges and middle against the other things' and the frame's within 6
pixels, a red line across the view and the chip flashing; lines it
already lined up with at the press are left alone. A red dashed rope
joins a picked follower and its leader, and an empty shot says to drag a
doll in.

On the rig canvas, Alt on a joint and a drag pulls a rope from it; letting
go over a piece hangs the joint's piece from it (over empty space makes
it a root), the same as double-clicking a joint and then clicking the
parent. A lone pivot dragged catches on the middle and edges of its
drawing within 6 pixels (`SnapToGuides`), and saving the rig to the
library says so in a toast.

Joint snap (`anim/joint_snap.h`, pure) also catches it on the drawn
limb: `MeasureLimbs` labels each solid shape of a drawing (alpha 8 and
up as the shape mesh counts it, holes closed in) with its long
direction, and `SnapPivot` picks, within the same reach, an end circle's
center or where the piece's and its parent's center lines cross first,
else the nearer of the middle of the width across the limb and the box
snaps. `RigCanvas` keeps measured drawings in a `LimbMemo`, draws the
caught line, middle and end circle, names it in the chip, and lets Ctrl
drag freely. Right click > Snap to shape or Snap all joints goes through
`RigManager::SnapJointsToShape` (edit/rig_snap.cpp), one undo step.
A joint circle (`RigPiece::joint_circle`, rig.json `joint_circle`) is
turned on with J (`ToggleJointCircle`, taking `JointColorAt` the first
time) and recolored by a palette pick (`SetJointCircleColor`). The
shared renderer draws it just before its piece, its radius measured
from the shown drawing each time (`JointRadius` over the
`ImageCache::limbs()` memo), never stored.

Find a command (Ctrl+/, Help menu, `CommandPalette`) is a box that
drops over the window and searches every menu action by name
(`MatchCommands`, pure: letters in order, word starts and side-by-side
letters score higher) with its key beside it; Enter runs the one picked,
a greyed one says why. The Keys and gestures sheet (F1, `ShortcutSheet`)
lists every menu key and the gestures no menu has. Something big going
well (an export done, a rig saved: `ToastManager::Cheer`) bursts red
pixels over the window (`SuccessBurst`: 24 in Full, 10 in Calm, none in
Off) and plays a chime; undo and redo tick (`UiSound`: quiet, on by
default, View > Sounds, silent with no audio output). The New project
size is four tiles drawn in the shape of each frame, and the export
dialog's format, range and size are rows of buttons (`ChoiceRow`).

The Cast panel's doll library is a row of cards (`DollCards`): a
picture of the doll (`DollPicture`, its default drawings stacked as
Krita had them, kept by the panel until the library changes), its name
and a badge (NEW ART, IN SHOT, in project). A card dragged onto the
stage puts the doll there (`PutDollsOnStage`, which takes it into the
project first when it is not in it, and which Put on stage uses too);
the stage frame glows while a card hovers. Shot chips in the strip are plain square chips: the first frame at
the left, then the name with the length in seconds under it, the shot's
colour as a bar along the bottom, a red outline when picked, a lift and
a lighter face under the pointer (`ShotThumbs`, shared with the shot
bin, never redrawn while an edit is running). Each cut on the reel wears
a badge with a letter for its transition (| cut, < > ^ v swipes, *
flash, ~ fade); one click on it opens the transition.

The inspector opens with a title line, a Teto red dot and what is
picked ("Head, frame 24", "3 picked", "Head, 2 dots"; `InspectorTitle`,
pure). Angle fields (Turn, Rest turn, Direction) have a small dial beside
them (`AngleDial`, `AddAngleRow`; the angle math is `DialAngle` in
`base/dial_angle.h`): drag to turn, Shift snaps to 15 degrees, Escape
puts it back; the field takes the value, so a drag is one undo step.
The wheel turns a dial 5 degrees a notch (Shift 1) and a double click
resets it to 0; a press alone changes nothing, so a double click never
jumps the needle.

The Collider box's list of ticked pieces sits in a scroll area capped at
about seven rows, so a doll with many pieces never pushes the rig panel
off the screen.

Group chips are `ChipButton`s: they ask for room for the whole name but
can shrink and cut it with "...", so a long group name never pushes the
inspector past its edge.

The Pose box is a small hardware panel (`pose_controls.h`) laid out by
use: the top row holds the two controls used most at one height, the
Move `TrackPad` (96 px tall, as wide as its cell; the grid slides under a
yellow puck, one unit per pixel) and the big Turn dial (72 px, ticked every
15 degrees), each in a sunken frame with its readout bar right under it at
the same width (x and y share the pad's bar), so numbers and control read
as one; then `Fader`s for Scale x and y, Skew and Opacity (lit red groove,
raised cap, a tick at the neutral value; wheel steps, double click
resets) on one label column and one readout width; then Lean and Swivel
as small 36 px dials, as they are rarely used. Rows are 8 px apart. Each control works the
box's own number field, shown beside it as a small readout you can type
in or scrub by its name, so the edit path is unchanged and a drag is one
undo step. `BoundDrag` holds what the pad and fader share (start values,
Set, Finish, Cancel on Escape).

Which boxes show is decided by `ShowFor` (`ui/inspector_show.h`, pure):
the pose for any pick; the layer's settings only for a whole layer or
doll, not a limb; saved poses only for a whole doll; groups on a doll
whose picked pieces have a warp grid with drag nodes or motions, or
while dots are picked or a group exists; the drag node and dot
animation while a dot is picked; the link only when linked. In the pose
box, Lean and Swivel show for a whole doll only and the Drawing picker
for a piece with more than one drawing.

Dropdowns are drawn by `SnapperStyle` as sunken full-width boxes with a
raised arrow button, and open a sunken list. A dial or slider under the
cursor leaves the wheel to the inspector while it scrolls (`scroll_guard.h`).

Most boxes below the title fold away under their heading (`Inspector::
AddFolded`, `FoldSection`; open at first, the fold kept in the settings
file); the Layer box and the ones whose title says how many are picked
do not. The tiles of a material and of a dot motion share `LoopTile` (a
tiny cartoon that loops only under the pointer; `KindTile`, `MotionTile`,
art in `material_art.h` and `motion_art.h`); the Dot animation box picks
how dots move on six tiles (None, Wave, Pulse, Breathe, Sway, Shiver),
outlined when the picked dots use them.

The side panel shows only the settings of what is picked: the shot
and its camera with nothing picked on the stage; pose, layer, motion
and saved poses for picked pieces or layers; the drag node and dot
animation panels for a picked warp dot, and the Link box only while a
pick follows something (`Inspector::Refresh`).

Panel kit. Every panel is built from `ui/panel_kit.h` so all of them
look like the Pose box: `MakePlate` (a centred raised plate, a control
cell's name), `MakeTitlePlate` (bold, hugging its text at the left edge,
a section's title), `Framed` (a sunken frame round a control and its
readout bar), `StackIn` and `InSlot` (flush columns, controls centred in
a fixed slot), `PlatedSection(title, body)` (a title plate over a sunken
frame with the body `theme::kIndent` in), `SettingsGroup` (a title plate
over a form indented by `kIndent`, no frame), `SettingsForm`, `GoldenRow`
(two cells on the golden ratio, `kGoldenLong` to `kGoldenShort`),
`GoldenSplit(total)`, `SharedRow`, `TileRow` and `EmptyHint` (a quiet line
saying why a list is empty and what fills it). `theme.h` holds the
spacing (`kIndent`, `kRowGap`, `kCellGap`) and `Em(widget)` for sizes in
ems. `FoldSection` is a raised title plate with a square triangle over a
sunken, indented frame; `AddTitle` makes a title plate; `SnapperStyle`
draws group boxes as square sunken frames. No rounded corners and no
style sheets: the look comes from frames and the style.
`ui/flow_layout.h` wraps a button bar onto more lines when narrow.
`ui/screen_fit.h` fits windows to any screen: `FitWindowToScreen` (85 %
of the screen, fitted again on a screen it does not fit), `GoldenDocks`
(the centre takes 0.618 of the width), `RememberLayout` (size, place and
docks per settings key; a place off every screen is refused),
`FitDialog(dialog, ems)` and `TabDocksWhenPortrait` (width / height under
1.2 tabs Cast behind the Inspector); the pure parts (`FittedRect`,
`GoldenDockWidths`, `IsPortrait`, `IsShownEnough`, `DialogSize`) take
plain numbers. `KeepWideEnough` (`scroll_fit.h`) asks for at most 30 %
of the screen's width.

The pick keys live in one place, `ui/pick_keys.h`: `ClickMode(mods,
is_picked)` gives the `PickMode` of a click (Ctrl flips and wins over
Shift, Shift adds, a plain click on something picked keeps the pick so it
can be dragged, and a release that did not drag narrows it), `BoxMode
(mods)` that of a box (Shift adds, Ctrl removes). `PickList::Install`
(`ui/pick_list.h`) makes any Qt item view follow them, with Ctrl+A,
Escape and Delete (`DeleteAsked`); the view's own selection mode becomes
NoSelection so only the filter writes the pick.

Who follows the rule: the stage (`PoseTool`: pieces, layers and warp
dots, Ctrl+box takes dots out), the rig canvas (pieces, joints, collider
dots), the timeline keys and reference rows, the shot strip, and every
list through `PickList` (the Cast panel's library and object cards and
layer list, the rig panel's piece list). A press that would take out or
narrow the pick waits for a release that did not drag (`PoseTool::Drag::
later`, `RigCanvas::later_joint_`, `TimelineView::later_keys_`); where a
drag moves only the thing grabbed (rig pieces, reference rows), a plain
click narrows at once. No widget reads Shift or Ctrl for a pick itself.

References are part of the one pick: `ReferenceManager` keeps a set of
picked references per shot with a focus (`PickMany`, `PicksIn`,
`PickedIn` for the focus). Made with the `SelectionManager` it drops the
stage's picked layers and pieces when references get picked, and drops
its own when the stage picks something. With references picked,
`ReferenceKey` (shared by the stage and the timeline) makes Ctrl+A pick
every reference row, Delete remove the picked ones as one undo step
(`RemovePicked`) and Escape let them go.

The Cast panel is three plated sections: Library (doll cards), Objects
(object cards, Add pictures and Background, which puts a picture under
every layer scaled to cover the canvas: `StageManager::AddBackground`)
and Layers (a `FoldSection`, folded at first as the timeline rows show
the same). Delete on cards takes the picked dolls out of the project as
one undo step (the library refuses ones still on a layer and says why).
The rig panel is plated sections too: Doll, Look, Skins and IK chains
with a doll; Joint and Warp with a piece; Collider only while the piece
is one. `PlateBox` (`ui/plated_box.h`) makes a box that shows and hides
itself (Look, Skins, Collider) its own plated section.

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
