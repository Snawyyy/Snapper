# Snapper architecture

Snapper is a puppet animator for music videos in the style of Execution
Clap. Characters are drawn and rigged in Snapper as Moho does it
(vector drawings, bones, smart bones, switches; see Characters), posed, cut into
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
| `model`  | Plain data: characters, project, shots, keys, reel | base         |
| `fsp`    | F's Plugins: the plugin list, their parameters, running one over a picture; the Line smoother's parameter table | base |
| `anim`   | Pure math: sampling keys, poses, bones, IK, links | model, fsp    |
| `io`     | Reading and writing character and project files   | model, fsp    |
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
- `media` knows nothing about characters or shots. It decodes audio and
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
(`TrackRef{shot, kind, layer}`: a layer's own move, a character's
poses, the camera, or an effect's strength), and `VisitTrack` /
`ReadTrack` hand over the channel whatever its value type. Posing, keys
and the selection all share this.

## Project data and undo

The project is a value. Undo keeps whole snapshots, not hand-written
inverse commands, so an undo bug cannot exist per feature.

Snapshots stay cheap through sharing: big parts of the project (each
shot, each character) are held as `std::shared_ptr<const T>`. An edit
copies only the part it touches and shares the rest with the previous
snapshot. Pictures are never in the project, only their file names, so pixels
are never copied.

History keeps at most `kMaxUndoSteps` steps and drops the oldest.

Each step also remembers what it changed (`StepScope`, `edit/step_scope.h`,
pure): which shots, or something shared (shots added or moved,
the cast, the reel, the song, the canvas, the name). `UndoIn(shot)` and
`RedoIn(shot)` act on the latest step of that shot only, taking it out
of the middle of the history when other shots were edited since
(`WithScopeFrom` copies just that shot back; the snapshots of the steps
newer than it, and the states waiting to be redone, take the shot as it
now is, so a later plain undo never brings it back). A step that
changed several shots, or the shared things, comes off only when it is
the newest. The window uses these in the Pose and Character tabs (the shot
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
  `PoseChanges`). The Pose, Video and Comp timelines all carry it in
  the ruler's top-left corner (`StepModeSwitch::CornerRect`); the mode
  is the one in `PlaybackManager`.
- The three timelines (`TimelineView`, `ReelTimeline`, `CompTimeline`)
  share their view and chrome, so they behave alike. `TimelineScale`
  (`ui/timeline_scale.h`) holds pixels per frame, the first frame in
  view and the length shown, and answers `XOf` and `FrameAt`. Zoom
  runs from `kMinPixelsPerFrame` to `kMaxPixelsPerFrame` in
  `kZoomStep` steps; `FitAll` shows the whole length and keeps doing
  so as it changes until the user zooms or scrolls (Comp starts so).
  `TimelineScale::Wheel` is the one wheel rule: plain scrolls time (a
  tenth of the view), Ctrl zooms round the cursor, Shift hands the rows
  back (only the Video tab has rows to scroll). `ViewKey`
  (`ui/timeline_keys.h`) is Ctrl+= and Ctrl+- (round the playhead) and
  Ctrl+0 (fit), and every timeline's menu ends with "Show all".
  While a play runs the scale pages along (`Follow`: the view jumps to
  start at the playhead when it leaves the view); a scroll by hand
  stops that until the next play (`BeginFollow`). The ruler
  (`PaintFrameRuler`, `ui/timeline_ruler.h`) counts frames with a
  taller tick and a number at every whole second, and the playhead
  (`PaintTimelinePlayhead`) is a 2 pixel line with its frame number in
  a tag on the ruler, settling on a frame (`PlayheadSettle`) on all
  three. `NudgeOf` reads Alt+Left and Alt+Right (Shift: ten): Pose
  nudges the picked keys (`KeyManager::Shift`), Video the picked clips
  (`ReelManager::MoveAll`), Comp the picked effect clips (`SetTiming`),
  each one undo step. All three drags use `DragFeedback`.
- A chip bar (`KeyChipBar`) floats over the picked keys on the
  timeline: Step, Ease, Line (pressed when all picked keys share it,
  `KeyManager::EaseOf`), On 2s, On 3s, and -1 f and +1 f (`KeyManager::Hold`:
  frames go in or out right after the last picked key and later keys
  slide). Each makes the same call as the right-click menu and is
  greyed with its reason; `KeyChipBar::SpotFor` is the pure placement.
- The playhead runs on one of two timelines: the shots one after
  another (Pose and Character tabs) or the reel (Video tab). Each keeps its
  own place, and posing always works at the shots' playhead
  (`PlaybackManager::FrameOn`). On the reel, animation mode jumps from
  cut to cut (`ReelCuts`).
- Playback is driven by the audio clock, so picture never drifts from
  the song. Without a song or a sound card it falls back to elapsed
  time.

## Characters

Characters are drawn and rigged on the Character tab and posed on the
Pose tab, the way Moho works. (The old PNG dolls are gone; an old
project opens with its doll layers left out and a note.)

- `model/vector.h`: a vector drawing is points (id, place, Moho style
  curvature 0 corner to 0.33 round, a width share for tapered lines),
  curves through point ids (open or closed; curves sharing a point are
  welded) and shapes (fill and line over curves, even-odd holes). The
  curve handles are worked out from the neighbours every frame
  (`anim/vector_path.h`, `HandlesAt`), so bent points stay round.
- `model/character.h`: a character is a tree of parts listed back to
  front, a parent before its children: Group, Bone (holds a skeleton
  that bends the parts inside), Vector, Switch (shows one child) and
  Image. Every id (parts, bones, points) comes from one counter,
  `next_id`, never reused. A part follows its bone part by `BindMode`:
  flexi (each point blended by the bones in reach, `FlexiWeights`),
  points bound by hand, riding one bone (layer binding), or none.
- A pose (`CharacterPose`) holds what differs from rest: each part's
  move, turn, scale, opacity, order, switch pick and point moves, and
  each bone's turn, push and stretch. A key on a `CharacterLayer` holds
  the whole pose (`Channel<CharacterPose>`), so one key is the drawing
  of a moment; the shared sampler blends them (`Lerp` in
  `anim/sampler.h`), picks hold until the next key.
- Lines and looks: a shape's line has a brush (`LineBrush`: solid, or
  pencil, ink, marker, chalk, dots dabbed along the curve by
  `render/line_brush.cpp` with grain fixed per dab, so still lines never
  shimmer), and a shape may use one of the character's named styles
  (`VectorStyle`, `Character::styles`): it then looks as the style says
  (`LookOf`, which every painter and hit test asks), so changing the
  style changes every shape using it. The pure style edits are
  `edit/character_styles.h`; `SetShapeLook` is the one rule for the Look
  panel (a styled shape edits its style). The Styles section
  (`CharacterStylesPanel`) makes, uses, lets go of, renames and deletes
  them; a deleted style's shapes keep its look as their own.
- Shaping tools: Line width (W) drags a point's width share, Delete
  edge (D) cuts one curve segment (`EdgeAt`, `DeleteEdge`: an open curve
  splits, a closed one opens, shapes it outlined go, loose points go),
  Magnet (M) moves the points near the press with a smooth falloff
  (`MagnetWeights`, `ShiftPoints`; [ and ] size its reach), all pure in
  `edit/vector_reshape.h` and drawn in `ui/character_canvas_tools.cpp`.
- Smart bones: a smart bone bends nothing; its turn plays its
  `SmartAction`, stops of pose deltas at turns, blended between stops
  (picks swap halfway) and added on top of the hand pose
  (`WithActions`). The Pose tab's Character box shows each as a slider.
- `anim/character_pose.h` (`PoseCharacter`) is the one place posing
  happens: parts placed down the tree, bones by forward kinematics, then
  points bent, giving every shown part back to front with its points in
  character space and every part's space. `anim/bone_ik.h` pulls a
  chain toward a target (CCD within limits, stopping at locked bones).
- `render/character_paint.h` draws a posed character (fills, lines,
  tapered lines, pictures) and hit tests it; the frame renderer and the
  stage use it for `CharacterLayer` like any layer kind.
- Edits: `edit/vector_edits.h` and `edit/character_edits.h` are the pure
  rig edits; `CharacterManager` wraps them in undo steps and owns the
  character library (`<data>/characters/<name>/character.json`, read
  and written by `io/character_file.h`, JSON in `io/character_json.h`).
  `CharacterPoseManager` keys poses on shot layers, every change built
  from the pure `edit/character_pose_edits.h` (`WithBoneDrag`,
  `WithPick`...), which the Character tab also uses to pose a smart
  bone's stop.
- UI: `ui/character_page.h` (tool box left of the canvas with icons
  from `ui/tool_icons.h`, `CharacterPartsPanel`,
  `CharacterCanvas`, `CharacterInspector`, view state in
  `CharacterSession`); on the Pose stage the picked character's bones
  are handles (`GrabKind::kBone`, `PoseTool::MoveBone`), drawn with the
  shared `ui/bone_glyph.h`; the inspector's `CharacterBox`. The Cast
  panel's Characters tab lists the library and the project's
  characters as cards (`ui/character_cards.h`, dragged out with
  `kCharacterMime`); New makes one and opens it, Edit and a double
  click open it on the Character tab, Put on stage and a drop on the
  stage add it (`PutCharactersOnStage`, `ui/character_drop.h`, taking it
  into the project first; `StageManager::AddCharacters`).

## Shots

A project is one song and an ordered list of shots on a master track.
Each shot has its own stage:

- a background colour, and layers bottom to top: characters,
  pictures, text for lyrics, storyboards and effects
  (`LayerContent`),
- a camera (pan, zoom, rotate, shake).

On the master track the shots follow each other with plain cuts;
transitions belong to the reel.

An effect layer (flash, fill, invert, zoom punch, halftone, glitch,
posterize) changes everything drawn below it, like an adjustment
layer. Effects are drawn in screen space, after the camera.

A shot also has a comp stack (`Shot::comp`, `model/comp.h`). The Comp tab works on the reel: its playhead is the reel's (the mode switch puts the playback on `Timeline::kReel` for Video and Comp), its stage draws `RenderReel`, and `ShotAtReel` (`anim/reel_timeline.h`) names the shot on top at the playhead, which `CompPage` then picks. The Comp
tab's finishing effects, and a Light (`Shot::light`, a keyed angle).
`FrameRenderer::RenderShot` runs the stack bottom first after every
layer (`render/comp.cpp`; what a target needs and its point space in
`render/comp_targets.cpp`), so every picture of the shot has it. Each
effect works on a target (`CompTarget{layer, colors}`): everything
below or one layer, narrowed by picked colors. The tab sets the target from the
stage pick when an effect is added (`CompLibrary` on the left), and
lists the pick's effects as folds (`CompEffects` on the right);
`CompManager::TargetFor` and `ShownFor` (`edit/comp_pick.cpp`) say
what a pick means. Each effect runs from `start` for `length` frames
of its shot (`kWholeShot` by default), timed over the shot's first
clip on the reel (`anim/comp_reel.h`). Effects on a layer run in the
shot's own pass, inside that span. Effects on the screen run once over
the finished reel picture wherever their span reaches, past the cut
too (`FrameRenderer::ApplyReelComp`, `render/comp_reel.cpp`), with the
layers of every shot showing there as their shape; a shot rendered on
its own (thumbnails) runs both. The Pose stage runs none. When an effect needs a target's own
pixels, `NeedsOf` says so and the layer pass paints each layer through
its own picture (`PaintForComp`), keeping the target as it shows in
the frame (cut where later layers cover it) and, for shape effects on
everything, the drawn pixels on clear. It also keeps each target's
cover (`CompPictures::covers`: the later layers), and what an effect on
a layer draws is cut by it,
so the target keeps its place in the draw order. The effect draws on a clear
picture (`render/comp_effects.cpp`, `render/comp_light.cpp`) and
`BlendOnto` (`render/blend.cpp`, the one blend function, W3C formulas,
Normal over see-through pixels) lays it over the frame through the
target's mask. Knobs are channels of doubles sampled by the one
sampler with a per-kind default (`anim/comp_knobs.h`); points live in
the target layer's space, or the canvas without the camera.
`CompManager` owns every edit (one named undo step each) and the
tab's view state: the picked effect, V and K (`CompShow`, passed only
by the Comp stage's own renderer) and each effect's last cost
(`CompReport`, with the plugin effects that refused to run).

One kind, `CompKind::kFsp`, is a plugin from F's Plugins
(`fsp/fsp_effects.h`, docs/features/fsp-effects.md). The effect keeps
the plugin's id (`CompEffect::plugin`) and its values in the knob slots
from `CompKnob::kParam0` on (at most `kCompParamSlots`; a bigger
plugin is refused), so numbers key, ease and sample like any knob;
toggles, choices and colors hold one value for the shot. Specs, words
and per-frame values come from the plugin's own parameters
(`anim/comp_fsp.h`); the panel's rows from `CompManager::RowsOf`. A
file saves the values by the plugin's parameter names
(`io/comp_fsp_json.cpp`); a plugin this Snapper lacks loads as a
placeholder that keeps the saved values as they were and draws nothing.
The renderer hands the plugin the target's picture as motion blur takes
it and lays the answer through the same blend and mask
(`render/comp_plugin.cpp`); a refusal skips the effect and the panel
says so.

The Line smoother (`CompKind::kLineSmoother`) is a rebuild of OLM
Smoother v2 (OLM Digital's After Effects plugin) from its manual, not
its code. Its parameters (Smoothness, Extra Smooth, Smooth Range, the
color key, Smoother Version, Gamma Correction and its colors) are a
table shaped like a plugin's (`fsp/olm_smoother_info.h`), so they use
the same value slots, rows, keys and file names as a plugin
(`ParamInfoOf` in `anim/comp_fsp.h`); a file without them loads with
the defaults. `render/comp_smoother.cpp` keys the color, moves the
frame into linear light (v2) and the gamma, and mixes each pixel with
the neighbours `render/comp_smoother_runs.cpp` names: an MLAA search
that reads each run of a stair step as part of a line through the
middles of the steps and gives a pixel the share of the color across
the edge the line covers; ends that turn back or into long sides
(corners, line caps) are left sharp. It works on the frame's own
pixels at any preview scale; picked colors name the lines it smooths,
a layer target keeps it to the layer grown by one pixel. Its answer
stands in for the frame (`StandInOnto` in `render/blend.cpp`): in
Normal the frame becomes it by Strength, alpha included, so it can
thin an edge or clear the keyed color; other modes blend only the
changed pixels.

A storyboard is a kind of layer (`StoryboardLayer`, `model/storyboard.h`,
the fifth alternative of `LayerContent`): a raster sketch of a shot's
plan made of cels, so the storyboard moves through the shot. Each cel
is a canvas-sized PNG that starts at a shot frame and holds until the
next cel. Being a layer, it takes its place in the stack, is ordered by
the Cast panel's layer list and hidden by the layer's eye
(`Layer::is_visible`); `StageManager::AddStoryboard` adds one,
`IsStagePicture` (`model/layer.h`) is false for it like for an effect
(no stage pick, no Comp target). It is laid over the canvas, with no
camera and no layer transform, by `FrameRenderer::PaintBoardLayer`
(`render/storyboard_paint.cpp`) at its place in the stack, at its own
`opacity`. A renderer draws it only after `SetStoryboardShown(true)`
(the Pose stage and the Video preview) or when the layer's `is_exported`
tick is on (the animatic); export, thumbnails and the Comp stage leave
it out otherwise. Pixels never enter the project: each edit saves a new
file ("<project> storyboard" beside the project file, or "storyboard"
in the app data folder while unsaved, `edit/storyboard_folder.cpp` on
the rule of `edit/sidecar_folder.h`), and the cel lists its path, so
undo is a path swap. In memory a path is absolute; the project file
writes one below the file's folder relative to it and reads it back
absolute (`io/storyboard_paths.cpp`, called by `WriteProject` and
`ReadProject`), so a project and its folder can move together. Save As
copies the pictures the history uses beside the new file and points
every project in the history at the copies, with no undo step
(`DocumentManager::CarryStoryboard`, `HistoryManager::MapProjects`). The pure edits (put, remove
and move cels, the layer an action means, fresh names) are
`edit/storyboard_edits.cpp`; `StoryboardManager` decides when to make
one, as one named undo step each ("Draw storyboard frame", "Edit
storyboard frame", "Move storyboard frame", "Delete storyboard frame",
opacity, export), and holds the picked cels. Drawing a cel reuses draw
mode: it hands `DrawManager` a `DrawJob` (the picture, the shot under
it without the layer, the cel before as a red tinted onion skin, shown
from the start, and a keep callback that Done calls), so the brush,
eraser, fill, picker, onion key D and stroke undo live in draw mode
once; draw mode draws nothing else. With no storyboard layer yet, Done makes one in the same
step. `ui/timeline_storyboard.cpp` (an event filter on the Pose
timeline, `timeline_storyboard_input.cpp` for its mouse and keys) draws
a dashed row per storyboard layer under the reference rows, with the
cels as keys (the eye is `ui/timeline_eye.cpp`, shared with the
reference rows); `StoryboardBox` in the inspector holds the opacity
fader, In export and the draw buttons; Alt+N and Alt+Shift+N start a
frame (Edit menu entries, `ui/storyboard_menu.cpp`, so the keys sheet and
Find a command list them; the stage's right click has the same two,
`ui/storyboard_actions.cpp`). Animation-mode stepping stops at
each cel start (`PoseChanges`).

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
row above the layer rows (`ui/timeline_references.cpp`), not counted in
the shot's length.

## Links

A layer can follow another layer's movement: right-click the leader on
the stage with the followers picked. A `Link` in the shot names the
follower and the leader (`LinkEnd{layer}`), the frame it was made at
and a strength. From then on the follower copies strength x how far
the leader's origin has moved in shot space since that frame, on top
of its own keys, so it never jumps onto the leader. Only x and y are
copied.

`LinkSolver` (anim) works this out: `LayerTransform` is the layer's own
transform (`anim/layer_transform.h`) shifted by its link's `Push`. A
leader that follows something passes that on, so chains work.
Everything that draws or hit-tests the stage asks the shot's solver
(`RenderShot`, `HitTest`, the stage geometry), so what is clicked is
what is seen. Keys stay as keyed; the pose tool moves by differences,
so a linked layer drags without jumping.

`LinkManager` refuses links that would loop back into themselves
(`WouldLoop`, a layer following itself included); a loop in a file
counts as no push. Links drop with their layers (`DropDeadLinks`) and
follow a duplicated shot's own layers.

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
key, link and layer span (`ShiftShotFront`, `edit/shot_timing.h`).
Every full clip of a resized shot follows, dragged edges move, partial
clips keep their in and length, later clips ripple (Shift keeps them put; plain trims ripple the same way, `RippleTrim`), and a clip
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

Sound can be recorded straight onto the reel, to plan timing by
mouthing a scene's noises. R in a sound track's header arms it
(`RecordManager::ToggleArm`, not saved); Rec in the Video bar
(`RecordButton`, greyed with the reason when there is no sound input)
counts in `kCountInBeats` seconds, then records the default input
(`AudioRecorder`, `media/audio_recorder.cpp`, 48 kHz mono 16-bit
through `QAudioSource`) from the reel's playhead while the reel plays.
Rec again, or the reel stopping, ends the take; Escape on the reel
throws it away. Each take is written as a WAV (`WaveFile`) into
"<project name> takes" beside the project file, or `takes` in the app
data folder for a project never saved, and laid from where it started
on the armed track (a free sound track when none is armed or it has no
room) through `ReelManager::AddSounds`: one undo step per take.

Spoken lines (the Voice tab of the Video side panel, always shown):
`VoiceGenManager` speaks a line in a cloned voice (a sample plus, for
a closer clone, the words it says) or a designed one (a description).
The speaking is `tools/voice_gen/voice_gen.py` (copied beside the
binary by CMake; its Python lives in `<data>/voice_gen/venv`, made by
`tools/voice_gen/setup.sh`), run by `VoiceWorker` (`media/`): one
long-lived process, a JSON job per line in, a JSON answer per line out
(loading, speaking, done, error), started on the first line since its
models take a while to load, and started again if it dies. A line is
written to a fresh `line NNN.wav` in a temporary folder; Add to
timeline copies the last one into "<project name> voices" beside the
project (`voices_used` in the data folder when unsaved; the folder rule
is `SidecarFolder`, shared with takes) and lays it at the reel's
playhead through `ReelManager::AddSounds`: one undo step. A sample can
come from the PC's own sound: `DesktopRecorder` (`media/`) reads
`parec -d @DEFAULT_MONITOR@` (Qt hides PipeWire monitor sources), at
most `kMaxSampleSeconds`. `VoiceLibrary` keeps voices across projects
in `<data>/voices/<name>/` (`sample.<ext>`, `voice.json`).

New shot here makes a blank shot sized to a stretch of the reel and
lays it there, as one step (`ReelManager::AddNewShot`,
`edit/reel_new_shot.cpp`). The stretch is a range marked with Alt and
a drag over empty track space, else `NewShotSpan`: the gap between the
markers around the click (cut back to the free space), else the free
space between the clips around it, else `kNewShotFrames`. It is offered
in an empty track's double-click menu, the track menu, and the "+"
tile over the shot list (at the playhead). A double click on a shot
clip (or a shot in the list, or Open in the Shot clip panel) emits
`VideoPage::OpenShotAsked` with the frame of the shot under it; Fit
shot to gap moved to the clip's menu.

The Video tab's side panel shows only what the pick needs
(`SidePanelsFor`): Text for text clips (and Subtitles when a cue is
among them), the Sound clip panel (volume and fades as faders) for
sound clips, the Shot clip panel (name, length through `ResizeShots`,
the Edges mode, Open) for shot clips, Subtitles with nothing picked,
and a hint otherwise, plus the always shown Voice tab (`VoicePanel`:
`ui/voice_panel.cpp`, `ui/voice_clone_box.cpp` for the Clone half,
`ui/audio_preview.cpp` to listen; its look:
`ui/wave_screen.cpp` a sound on a phosphor
screen, click to listen from there, `ui/led_meter.cpp` the recording
meter, `ui/hero_button.cpp` the big red Generate, `ui/bevel.cpp` the
raised or sunk edge every self-painted widget shares). `ShowSide` and `SidePanelsFor` live in
`ui/video_page_side.cpp`. The reel's tracks scroll up and down (a scroll
bar, Shift and the wheel, or the wheel over the track names) instead of
growing the timeline; the Video bar wraps (`FlowLayout`).

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
test. Each layer kind is drawn by `PaintLayer` (`render/layer_painter.h`)
through the shot's `LinkSolver`. Each renderer has its own
`ImageCache` of pictures read from files; nothing is shared across
threads. Preview may render at a
smaller scale; effects scale their sizes with it so they look the same.

Draw order is keyed (`LayerPose::order`, `PoseField::kOrder`). A key
holds from key to key: `Lerp` copies it from the earlier key, never
blends it. On a layer's own move it moves the whole layer N places in
front (above 0) or behind (below 0) of its place in the shot's stack,
past any kind of layer, stopping at the front or back, ties keeping
stack order. That is `LayersInDrawOrder` (`anim/layer_order.h`, counts
only layers showing at the frame): `RenderShot`, `RenderLayers`,
`RenderArea` and `HitTest` all walk it, so the reel, export, stage and
a click agree. `PoseManager::SetField` (typed number),
`PoseManager::Shift` (wheel steps) and `PoseManager::StepOrder` (Ctrl+]
and Ctrl+[, Edit menu entries in `ui/order_menu.cpp`) key it as one step
named by `PoseManager::OrderName` ("Draw order Bo"); the Pose box has
an Order field (`PoseBox::SetUpOrder`, double click resets through
`ui/reset_on_double_click.h`).

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

`RenderReel` draws a reel frame: shot clips through `RenderShot`, video
clips fitted to the canvas, keeping their shape. Over a transition both
clips are drawn over the tracks below, each finished with its own
screen effects like a video file, and mixed by `Combine`; on such a
frame no effect runs over the mix, so an effect stretched past its cut
skips it. Pictures
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

The CPU is enough for flat-colour characters at 1080p. If preview at full
size ever drops below 24 fps, the upgrade is to move `FrameRenderer`
onto the GPU behind the same interface; nothing else would change.

## Managers

One manager per concern, each owning its state and reached through a
small interface. `app` builds them once, in dependency order, and they
are destroyed in reverse.

| Manager                | Concern                                        |
|------------------------|------------------------------------------------|
| `HistoryManager`       | Current project, undo, redo, dirty state       |
| `CharacterManager`     | Character rig edits (parts, drawings, bones, binding, switches, smart actions) and the character library |
| `CharacterPoseManager` | Character poses keyed on shot layers: bone drags, picks, smart bone turns, reset |
| `DocumentManager`      | New, open, save, autosave, recent files, the per-file outside switch |
| `ShotManager`          | Shots on the master track: order, length, look |
| `ReelManager`          | The reel: clips, tracks, cuts, transitions, sound clips, volume, fades, Mute and Solo |
| `SoundManager`         | The reel's sound files decoded and their waveforms; the mix; which clip a shot borrows its sound from |
| `RecordManager`        | Recording sound onto the reel: the armed sound track, the count-in, the take from the playhead while the reel plays, its WAV beside the project, laid on as one step |
| `VoiceGenManager`      | Spoken lines: clone a sample or design a voice through the voice worker, the last line, samples from the PC's sound, adding a line to the reel at the playhead as one step |
| `VoiceLibrary`         | Voices saved across projects: sample, words, language, description |
| `StageManager`         | What is in a shot: characters, pictures, text, storyboards, effects, flip |
| `LinkManager`          | Which things copy another's movement, how much |
| `SelectionManager`     | What is picked: shots, layers, keys, clips     |
| `PoseManager`          | Layer pose edits: move, turn, scale, skew, opacity, draw order, camera, effect strength |
| `DrawManager`          | Draw mode: the picture a `DrawJob` hands in (a storyboard cel), its tools and stroke undo; Done hands it back to the job's keeper |
| `StoryboardManager`    | Storyboard layers: draw a cel (a `DrawJob` for draw mode), save it as a PNG, move and delete cels, opacity and the export tick, one step each; the picked cels |
| `KeyManager`           | Keys on the timeline: move, space, ease, retime, bake a timing chart |
| `TimingChartManager`   | The timing chart session: picked thing and frames to chart, ticks, live preview in one `EditScope`, Apply or Cancel |
| `PlaybackManager`      | Playhead on either timeline, play, loop, song, the sound each timeline plays |
| `ExportManager`        | MP4, GIF, PNG frames, WebM, MOV export, threaded; sound only as WAV |
| `MotionManager` (`ui`) | How long and how softly the UI moves: the Motion table, Full / Calm / Off, saved in settings |
| `ToastManager` (`ui`)  | Toasts: the queue, their time, stacking, dismissal and the Undo they offer |
| `TimingChartDialog` (`ui`) | The floating timing chart window (Shift+T, or Define timing chart... in the stage and timeline menus); its strip only draws the ticks and forwards drags to `TimingChartManager` |
| `SpaceKeysShortcuts` (`ui`) | The 1 to 4 window shortcuts and the Space evenly menu; both call `KeyManager::Space` through `RequestSpaceKeys` |
| `DragPlayhead` (`ui`) | The playhead's place before a key or clip drag: the drag carries the playhead, Escape puts it back, no history |
| `PaintClip` (`ui`) | RAII clip so timeline keys, bars, grid and playhead paint only in the frame area, never over the names |
| `RefusalShaker` (`ui`) | The shake and the reason on a greyed control clicked anyway |
| `SettingsManager` | Ctrl+Shift+C / V: copy everything about the picked layer except its pose at the playhead (a character's flip, text look, effect kind and colour) and paste it onto layers of the same kind as one step; `SettingsActions` (`ui`) are its menu entries |
| `StepScope` (`edit`) | What an undo step changed, for per-shot undo (see Project data and undo) |
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
else; a newer edit takes the button away. Pinned hints and
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

Handles look small and grab big, the same on the stage, the Character canvas
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
and the stage flashes white the drawn pixels of each layer that
changed (the frame renderer draws just those), with a faint red copy fading where
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
on the Character tab). Both forward clicks to `PlaybackManager` and
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

Every list of things (stage layers, the layer list, timeline keys,
shots, reel clips, character parts, library characters) picks the same way: click picks
one, Shift adds, Ctrl flips one in or out, a box on empty space picks
what it touches (Shift adds, Ctrl takes out), Ctrl+A picks all, Escape
or a click on empty space clears. `Combine` in `selection_manager.h` is
the one rule they share.

A change made with many things picked is one undo step and is added to
each thing's own value, so they keep their differences: x 10 and x 0
moved by 5 become 15 and 5. Managers offer these as `...All` or
`Shift...` methods; widgets show the focused pick's value and send the
difference.

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
picked it lands on the playhead's frame as before. Tab and Shift+Tab step the pick through the layers (`SelectionManager::StepPick`). A move drag
catches on guides (`SnapToGuides`, `anim/guides.h`, pure): its box's
edges and middle against the other things' and the frame's within 6
pixels, a red line across the view and the chip flashing; lines it
already lined up with at the press are left alone. A red dashed rope
joins a picked follower and its leader, and an empty shot says to drag a
character in.

Find a command (Ctrl+/, Help menu, `CommandPalette`) is a box that
drops over the window and searches every menu action by name
(`MatchCommands`, pure: letters in order, word starts and side-by-side
letters score higher) with its key beside it; Enter runs the one picked,
a greyed one says why. The Keys and gestures sheet (F1, `ShortcutSheet`)
lists every menu key and the gestures no menu has. Something big going
well (an export done, a character saved: `ToastManager::Cheer`) bursts red
pixels over the window (`SuccessBurst`: 24 in Full, 10 in Calm, none in
Off) and plays a chime; undo and redo tick (`UiSound`: quiet, on by
default, View > Sounds, silent with no audio output). The New project
size is four tiles drawn in the shape of each frame, and the export
dialog's format, range and size are rows of buttons (`ChoiceRow`).

Shot chips in the strip are plain square chips: the first frame at
the left, then the name with the length in seconds under it, the shot's
colour as a bar along the bottom, a red outline when picked, a lift and
a lighter face under the pointer (`ShotThumbs`, shared with the shot
bin, never redrawn while an edit is running). Each cut on the reel wears
a badge with a letter for its transition (| cut, < > ^ v swipes, *
flash, ~ fade); one click on it opens the transition.

The inspector opens with a title line, a Teto red dot and what is
picked ("Bo, frame 24", "3 picked"; `InspectorTitle`, pure). Angle fields (Turn, Rest turn, Direction) have a small dial beside
them (`AngleDial`, `AddAngleRow`; the angle math is `DialAngle` in
`base/dial_angle.h`): drag to turn, Shift snaps to 15 degrees, Escape
puts it back; the field takes the value, so a drag is one undo step.
The wheel turns a dial 5 degrees a notch (Shift 1) and a double click
resets it to 0; a press alone changes nothing, so a double click never
jumps the needle.

The Pose box is a small hardware panel (`pose_controls.h`) laid out by
use: the top row holds the two controls used most at one height, the
Move `TrackPad` (96 px tall, as wide as its cell; the grid slides under a
yellow puck, one unit per pixel) and the big Turn dial (72 px, ticked every
15 degrees), each in a sunken frame with its readout bar right under it at
the same width (x and y share the pad's bar), so numbers and control read
as one; then `Fader`s for Scale x and y, Skew and Opacity (lit red groove,
raised cap, a tick at the neutral value; wheel steps, double click
resets) on one label column and one readout width; then the draw
Order. Rows are 8 px apart. Each control works the
box's own number field, shown beside it as a small readout you can type
in or scrub by its name, so the edit path is unchanged and a drag is one
undo step. `BoundDrag` holds what the pad and fader share (start values,
Set, Finish, Cancel on Escape).

Which boxes show is decided by `ShowFor` (`ui/inspector_show.h`, pure;
what the pick has beyond the stage pick comes in `PickExtras`): the
shot and its camera with nothing picked; the pose and the layer's
settings for any pick but a storyboard; the Character box for a
character; References only while one of the shot's references is
picked; the Storyboard box while a storyboard layer or its frames are
picked; the link only when linked.

Every inspector box is built from the panel kit like the Pose box: each
group of settings is a plate over a sunken frame (Name, Length and
Background in the Shot box; Name, Frames and a plated Text section in
the Layer box; a Reference plate with Look and Plays sections; Follows
with Unlink in the Link box), numbers are
small readouts scrubbed by their name or faders, rows are
`theme::kRowGap` apart, and what cannot act is greyed with the reason.

Dropdowns are drawn by `SnapperStyle` as sunken full-width boxes with a
raised arrow button, and open a sunken list. A dial or slider under the
cursor leaves the wheel to the inspector while it scrolls (`scroll_guard.h`).

Most boxes below the title fold away under their heading (`Inspector::
AddFolded`, `FoldSection`; open at first, except the Camera, right under
the Shot box, which starts closed; the fold kept in the
settings file); the Layer box and the ones whose title says how many
are picked do not. The inspector asks for the width its boxes need, but
never more than its share of the screen (`KeepWideEnough`).

The side panel shows only the settings of what is picked: the shot
and its camera with nothing picked on the stage; pose, layer and
character for picked layers, the Link box only while a
pick follows something, and a reference's settings only while it is
picked (`Inspector::Refresh`, which also follows the ReferenceManager).

Accents. What is on, picked or focused wears Teto red (`ui/snapper_accents.h`, drawn by the style): a strip over the picked tab, a strip at the foot of a toggled button (Play while playing), red marks in check boxes and radio buttons, a ring round the field that has the keys, and a strip down the left of a title plate (`MakeTitlePlate` sets the `titlePlate` property the style reads).

View camera. The Comp stage and the Video preview zoom with the wheel toward the pointer and slide with a middle button drag (Shift+1 fits), through `ViewCamera` (`ui/view_camera.h`): the zoom and slide on top of each widget's own fit. The Pose stage and the Character canvas keep their own copies of the same idea; on Pose the plain wheel turns the pick, so Ctrl+wheel zooms there. Pictures are rendered at most one frame pixel per pixel and magnified past that.

Grips. Splitter handles and dock separators are `kGripExtent` wide with ridges along them (`DrawGripLine`, `ui/snapper_style.h`) and turn yellow under the pointer or while dragged. Pages use `GripSplitter` (`ui/grip_splitter.h`), whose handles follow enter and leave themselves, light after `kGripDwellMs` of rest, put the default shares back on a double click and keep the sizes they were dragged to (as parts per 10000, under `splits/<key>` in the settings file). Dock separators have no hover state or double click in Qt, so `DockGrips` (`ui/dock_grips.h`) watches the main window: it finds the separator under the pointer, publishes the lit one in the window's `litGrip` property for the style, resets a dock to its golden width on a double click and keeps dragged widths under `docks/<name>`.

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

Who follows the rule: the stage (`PoseTool`: layers), the Character
canvas, the timeline keys and reference rows, the shot strip, and every
list through `PickList` (the Cast panel's character cards and layer
list). A press that would take out or
narrow the pick waits for a release that did not drag (`PoseTool::Drag::
later`, `TimelineView::later_keys_`); where a
drag moves only the thing grabbed (reference rows), a plain
click narrows at once. No widget reads Shift or Ctrl for a pick itself.

References are part of the one pick: `ReferenceManager` keeps a set of
picked references per shot with a focus (`PickMany`, `PicksIn`,
`PickedIn` for the focus). Made with the `SelectionManager` it drops the
stage's picked layers when references get picked, and drops
its own when the stage picks something. With references picked,
`ReferenceKey` (shared by the stage and the timeline) makes Ctrl+A pick
every reference row, Delete remove the picked ones as one undo step
(`RemovePicked`) and Escape let them go.

The Cast panel is two tabs stacked in one area, drawn as index divider
tabs (`DividerTabs`) (the open one kept in the settings file as
`cast_tab`): Characters (character cards, New, Edit, Put on stage; see
Characters) and Layers (the shot's layers, top first; Add puts a
picture, a background under every layer scaled to cover the canvas
with `StageManager::AddBackground`, text, a storyboard or an effect).
Under the tabs a tape player (`CassetteDeck`) follows the playhead: its
reels turn with every frame moved, the tape winds from left to right
over the timeline, and a drag across it scrubs. A glass door in front
(`GlassDoor`) slides right by its handle and tips on a spring when
pulled up or down. Picture files dropped on the stage become picture
layers (Alt: references).

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
tools/               build checks (layer check), the voice worker
cmake/               shared CMake functions
```
