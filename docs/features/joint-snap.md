# Joint snap and joint circles

Status: planned.

Needs: nothing for the snapping; the [floating palette](palette.md) for circle colors.

Placing a joint should land it on the limb's true center, and bending it should not open a gap. Today a lone pivot only catches on the middle and edges of the drawing's box (`ui/rig_canvas_snap.cpp`), not on the shape drawn inside it. From the measured width, an optional circle fills the joint so bent limbs look connected, the overlapping-circles joint used in cutout animation.

## Snapping to the shape

- Width middle: while a joint is dragged on the rig canvas, Snapper measures straight across the drawn limb at the cursor (across the limb's long direction, counting pixels that are visible, alpha 8 and up, the same rule the mesh uses) and catches on the middle of that width.
- End center: near a rounded end, it catches on the center of the end's circle, half a width in from the tip. That is the spot where two limbs turn without the end showing a corner.
- Overlap center: when the joint's piece and its parent overlap, it also catches where their two center lines cross.
- These join the box snaps that exist today; the nearest one within the same 6 screen pixels wins.
- What it caught is drawn while dragging: a thin red line across the limb from edge to edge, a dot at the middle, and for an end, the end circle outlined. A chip names it ("Width middle", "End center").
- Hold Ctrl while dragging to place freely with no snapping.
- Snap pivot: right click a joint and choose Snap to shape to move an existing joint to the nearest shape snap, one undo step. Snap all joints does every joint of the doll at once, listing what moved.

## Joint circles

- A circle drawn at the joint, its diameter the limb's width measured there, so the bend shows a round elbow instead of a gap or a corner.
- On by a toggle per joint in the joint's settings (proposed key: J on a picked joint). A doll setting turns it on for new joints.
- Color: picked from the [floating palette](palette.md), so it is one of the doll's own colors. By default it takes the most common color at the joint, which is usually the limb's fill.
- Outline: optional, with its own color and width from the palette, drawn only on the outer side of the bend so it continues the limb's outline instead of crossing the fill.
- Size: follows the width automatically; a Size fader nudges it (90% to 120%) for limbs that taper at the joint.
- Draw order: between the two pieces, just under the upper one, so it fills the gap without covering either drawing's lines.
- It moves, turns and scales with the joint, follows drawing swaps (a swapped drawing with a different width gets a circle of its width), and shows in preview and export.
- Look: drawn smooth in Smooth looks and as hard pixels in Crisp, to match the doll ([Smooth warp](smooth-warp.md)).

## Undo, saving and the rest of Snapper

- Every snap drag, toggle and color change is one undo step, named ("Undo Joint circle elbow").
- Joint circles and their colors save in the rig file, so they come with the doll.
- Mirror copies circles and colors to the twin joint.
- A Krita re-export that changes a limb's width updates the circle by itself, since the size is measured, not stored.
- Skins can set a circle color per skin ([Skins](skins.md)).

## Edge cases

- No drawn pixels under the cursor (a gap, a hollow shape): no width snap, only the box snaps.
- A limb drawn as outline only (empty inside): width is taken from outer edge to outer edge, so the circle still fills the joint.
- Two separate shapes in one drawing: the width is measured on the shape under the cursor only.
- Warped pieces: the circle stays at the joint and keeps its size; the bend does not stretch it.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| Ctrl + drag | Rig canvas, joint | Place freely, no shape snapping |
| J | Rig canvas, joint picked | Joint circle on and off |

## Open questions

- [ ] Joint circles: on by default for new dolls, or off until turned on per joint?

## Not in the first version

- Joint shapes other than circles (ovals for flat joints), and auto-detecting the limb's taper to size the circle per frame.
