# Editing outside the frame

Status: planned. Reported by the owner: limbs outside the shot's frame cannot be picked or edited.

A doll walking in from off screen, an arm swinging past the edge or a prop waiting to be thrown in all live partly outside the frame. Today the stage paints only the rendered frame picture (`StageView::paintEvent` draws one canvas-sized image), so anything past the frame edge is invisible on the stage and there is nothing to click. The stage should work like a pasteboard: everything is drawn and editable, and the frame only marks what the camera will export.

## Seeing outside the frame

- The stage draws the whole shot, not only the canvas: parts past the frame edge show dimmed (about 40% strength) over the dark stage face, so they read as "not in the shot" at a glance.
- The frame edge is a thin line, with the outside darkened a little more, the same as a camera viewfinder.
- A toggle (proposed: O) hides the outside area for a clean view of only what exports. Remembered per project.
- Zoom out and Fit (the existing stage zoom) take the off-frame content into account: a new Fit all fits everything in the shot, Fit frame fits only the frame as today.

## Picking and editing

- Picking, hover outlines, handles, dots, the rope and box select all work outside the frame exactly as inside, since the hit test works in shot space; check first whether anything else bounds clicks to the frame, then mostly only what is drawn changes.
- Handles and the red pick outline are drawn at full strength even outside the frame, so a picked off-frame limb is clear.
- Dragging a piece across the frame edge never stops at the edge, and guides keep catching on the frame edges (an arm lined up with the frame edge to exit cleanly).
- Warp dots, jelly and cloth keep running outside the frame, so a limb swinging back in arrives with its physics already moving.

## The camera

- Off-frame content is in shot space, so it follows the shot camera: a camera pan brings an off-frame doll into the frame as expected.
- Camera shake does not move the dim outside drawing, only the frame contents, so it does not flicker while posing.

## Speed

- The outside area is drawn at the same preview scale as the frame and only where something is, so an empty pasteboard costs nothing.
- Export is unchanged: it renders only the frame.

## Edge cases

- A doll entirely outside the frame still appears in the timeline and can be picked from its layer row, as today.
- Very far away content (a prop dropped 10 screens away): Fit all still reaches it, and a small arrow on the frame edge points toward picked things that are off the visible stage.
- Text and effect layers: text past the edge shows dimmed like dolls; effects stay screen-space and only affect the frame.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| O | Stage | Show or hide the outside of the frame |

## Open questions

- [ ] How dim should the outside be, and should it be a setting?
- [ ] Should Fit all or Fit frame be the default when a shot opens?

## Not in the first version

- Per-layer locking so off-frame props cannot be picked by accident.
