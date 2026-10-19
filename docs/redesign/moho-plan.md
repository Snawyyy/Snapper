# Moho style characters: the plan

Status: phases 1 to 7 built and the converter (phase 10's first half)
in, 2026-10-19; physics and Comp ports and the old code's removal are
left. Calls made: whole pose keys, Moho curvature (not Bezier handles),
no frame by frame or SVG yet, library dolls convert on first use.

The owner's calls (2026-10-19):

- The Moho rig replaces the PNG dolls. Old projects still open: a
  converter turns each old doll into a character (see Phase 10).
- Art is drawn in Snapper as vectors. Krita is no longer the art path.
- First four parts: vector drawing, a bone rig, smart bones, switch
  layers.

## What changes in one picture

Today a doll is a set of PNG pieces from Krita, joined at pivots, each
piece warped by a dot mesh, keyed as one pose. After the change a
character is a tree of layers, like Moho:

- Group layer: holds other layers, has its own move, turn, scale.
- Bone layer: a group that also holds a skeleton. Layers inside it are
  bent by its bones.
- Vector layer: points, curves between them, shapes (fills and strokes)
  made from the curves. Points are what bones bend and keys move.
- Switch layer: a group that shows one child at a time, keyed (mouths,
  hands, eyes).
- Image layer: a picture, bound to one bone or bent by bones through a
  mesh (for photos, props and old PNG dolls).

The vector points take over the job of the warp dots: drag, cloth and
jelly physics, colliders, point motions and groups all run on points.

## What stays, what goes

Stays as it is: shots, the reel, sound, text and subtitles, transitions,
references, storyboard layers, Comp (targets move from pieces to layers
and shapes), links, the timeline view, playback, export, undo, the
panel kit and the pick rule.

Changes shape, same idea:

- Pieces become layers in the character tree.
- Joints and IK chains become bones (`anim/ik.cpp` is reused).
- Drawing swaps and Skins become switch layers.
- Smart dials become smart bones (the dial stays as how a smart bone is
  shown in the inspector).
- Warp dots become vector points; the mesh code goes.
- Draw mode (pixel brush) becomes the vector tools. Storyboard keeps
  the pixel brush.
- The palette reads colors from shapes instead of scanning PNGs.
- Colliders become shapes.

Goes: `art.json` and the Krita exporter, Doll to Krita (`.ora`), shape
mesh and grid building, mesh carry, Smooth warp and the upscaler, joint
snap and joint circles, the old doll library format.

## Order of work

Build the new character beside the old doll, switch the app over, then
delete the old code last. That way the app opens and runs at every step.
Each phase is one branch, built and merged before the next.

### Phase 1: vector drawing data and drawing it

- `model/vector.h`: `VectorPoint` (id, place, smooth or corner, two
  handles), `VectorCurve` (point ids, open or closed), `VectorShape`
  (curves, fill color, stroke color, stroke width, width per point,
  hidden edges), `VectorDrawing`.
- `render/vector_paint.cpp`: shapes to a `QPainterPath`, filled and
  stroked, antialiased, in the one renderer.
- `io/vector_json.cpp`: save and load.
- Tests: `anim/vector_tests`, `io/vector_json_tests`,
  `render/vector_render_tests`.

### Phase 2: vector tools (replace draw mode)

- `VectorManager` owns every vector edit, through `HistoryManager`.
- Tools, one key each: Add point (click, drag for a curve), Freehand
  (a stroke turned into few points by curve fitting), Pick points (box,
  pick rule), Move, Turn and Scale points, Curvature (smooth or corner,
  handles), Make shape (fill a closed area), Paint bucket, Stroke width,
  Hide edge, Delete, Weld (join two points).
- Snap points to points. Onion of the frame before and after.
- Tests: `edit/vector_edit_tests`.

### Phase 3: the character tree

- `model/character.h`: `Character` (name, root layer tree) and
  `CharLayer` (a variant of Group, Bone, Vector, Switch, Image) with
  its rest move, turn, scale, flip, opacity and draw order.
- A shot layer of kind `CharacterLayer` replaces `DollLayer`.
- Library: a character is a folder with `character.json`. Cast cards,
  drag to the stage and library folders keep working.
- Rig tab becomes the character tab: a layer tree panel (add, rename,
  reorder by drag, eye), the vector tools on the picked vector layer.
- `CharacterManager` owns tree edits.

### Phase 4: keys

- One channel per thing that can move: a layer's move, turn, scale,
  opacity, order; each point's place; each bone's turn, place, scale;
  each switch's pick. The shared `Channel<T>` and sampler are reused.
- Open question 1 decides how a key is set.

### Phase 5: bones

- `model/bone.h`: bones on a bone layer (place, turn, length, parent,
  strength, turn limits, IK lock).
- Binding, as in Moho: flexi binding by default (each point follows the
  bones near it, weighted by distance and strength), point binding
  (points tied to one bone by hand) and layer binding (a whole layer
  rides one bone).
- `anim/bone_pose.cpp`: bones to world, then points bent by them. Pure,
  most tests live here.
- IK by dragging a bone tip, with targets and locks; `anim/ik.cpp`.
- Bone dynamics (lag and bounce) from the drag spring.
- `BoneManager` owns bone edits; `PoseManager` owns posing drags.
- Tests: `anim/bone_tests`, `edit/bone_edit_tests`.

### Phase 6: switch layers

- A switch layer keys which child shows (holds until the next key).
- Child order and names from the layer tree; a picker on the stage and
  in the inspector; keys on the timeline row.
- Skins become saved switch picks across many switch layers.

### Phase 7: smart bones

- A smart bone drives an action: a short timeline of keys on any
  channels under its bone layer (points, layers, switches, other
  bones), played by the bone's turn between two angles.
- An action edit mode: pick the smart bone, set its angle, move points
  and layers, a key is saved in the action at that angle.
- The inspector shows each smart bone as a dial (today's Smart dials
  box), so it can be keyed without touching the stage.
- `DialManager` becomes `ActionManager`; `model/smart_dial.h` becomes
  `model/action.h`.

### Phase 8: physics and points

- Drag, cloth and jelly run on vector points instead of warp dots; the
  solvers stay, only their input changes.
- Colliders are shapes; point motions and dot groups work on points.

### Phase 9: the rest of the app

- Comp targets: layers and shapes of a character, picked colors.
- Links and Hold: hold on a bone.
- Mirror: flip a character, mirror bones named `_L` and `_R`.
- Poke, copy and paste a pose, saved poses, the palette.

### Phase 10: old projects in, old code out

- Converter: each PNG piece becomes an image layer bound to a bone at
  its pivot, its drawings a switch layer, its smart dials smart bones.
  Old projects open as characters and look the same, minus mesh warps.
- Then delete the doll code, the Krita tool, `.ora`, the mesh and the
  upscaler, and update `ARCHITECTURE.md` and `docs/feature-map.md`.

## Open questions for the owner

1. Keys. Today one key on a doll keys its whole pose (drawing style).
   Moho keys each channel on its own. Keep whole pose keys, or key each
   channel? Plan default: keep whole pose keys, with a switch to key one
   channel.
2. Curves. Bezier handles (like Moho 13 and SVG) or Moho's old
   curvature number per point? Plan default: Bezier handles.
3. Frame by frame vectors: a vector layer with a new drawing per frame,
   like Moho's frame by frame layer? Plan default: later.
4. Import SVG later as a side door? Plan default: later.
5. Old dolls in the library (not in a project): convert them all on
   first start, or one by one when used? Plan default: on first use.
