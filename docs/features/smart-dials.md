# Smart Dials

Status: planned. Inspired by Moho's Smart Bones and switch layers.

A Smart Dial is a named control on a doll that moves several pieces at once between saved stops. Between stops the moves blend and the drawings swap, so one dial sweep plays a head turn, a blink or a mouth shape. All raster, no mesh needed.

## Making a dial (Rig tab)

- A Smart Dials box in the Rig tab, shown when a doll is picked. A "+" tile makes a new dial and asks for a name, in place, no dialog.
- Start from a template tile: Head turn (left, front, right), Blink (open, half, closed), Mouth (rest plus shapes), or Blank. A template only names the stops; the art is yours.
- The dial drives the pieces picked when it is made. Shift click adds pieces, Ctrl click removes, the same as picking anywhere else. Driven pieces show a small dial badge on the stage while the box is open.
- Stops: turn the dial to a stop, pose the pieces (rotate, move, scale, skew, opacity, drawing, draw order, warp dots), press Set stop. A stop shows as a tick on the dial with its name. Up to 8 stops per dial.
- Add a stop between two others by turning to that spot and pressing Set stop. Right click a tick to rename, move, copy its pose to another stop, or delete it.
- Rest stop: one stop is marked rest (a filled tick). The dial sits there by default and double click returns to it.
- Drawing swap point, per dial: swap at the halfway point between stops (default, good for turns) or exactly at the stop (good for blinks).
- Try it live: the dial in the Rig tab turns the doll on the stage without keying, so the stops can be checked by scrubbing the dial.

## Using a dial (Pose tab)

- Dials show in the Pose box under a Smart dials fold when the whole doll, or any piece a dial drives, is picked. Picking a piece shows only the dials that move it.
- Same feel as the other dials: drag or turn the wheel, double click to reset to rest, a value chip shows the stop name or the blend ("Front 60% Right").
- Snaps to stops when near them; hold Alt while dragging to turn freely without snapping, hold Ctrl for fine steps.
- Several dolls picked: dials with the same name turn together, so a crowd can blink at once. Dials missing on some dolls are greyed with a tooltip naming them.
- Hover a dial and the pieces it drives get a light red outline on the stage, so you know what it will move before touching it.

## Keys and the timeline

- A dial value is keyed with the doll, like every other part of the pose: turning a dial at a frame makes or updates that frame's key.
- Keys hold unless set to ease, as everywhere in Snapper. When eased, the blend follows the key's easing, and the drawing swaps at the swap point.
- Each dial gets its own row under the doll's row in the timeline, folded by default, so a blink can be retimed without touching the body keys. Its key chips show the stop name.
- Copy and paste of a pose carries its dial values. Saved poses store dial values too, so a saved "surprised" pose brings its mouth and brows back.
- Bake: right click a dial row and choose Bake to keys, which writes the plain piece keys the dial makes and removes the dial keys, for hand-tweaking every frame.

## Mixing with hand posing

- The dial poses the pieces first and hand posing adds on top, so after setting a head turn the head can still be tilted by hand without fighting the dial.
- Two dials on one piece: moves add up; for the drawing, the dial lower in the list wins. Dials can be dragged to reorder.
- Jelly, cloth and drag dots react to a dial move exactly as to a keyed move, because the dial only changes the pose they read.
- IK and links read the posed result, so a dial that moves a shoulder carries the arm along.

## Undo, saving and Krita

- Every dial drag is one undo step, named ("Undo Head turn dial"). Escape during a drag puts it back. Setting, moving or deleting a stop is one step each.
- Dials save in the doll's rig file (rig.json), so they come with the doll into every project, and Save rig writes them back to the library.
- Stops name drawings by their Krita layer name, not number, so re-exporting from Krita in a new order does not break them. This needs the exporter to keep layer names (see [Doll to Krita](doll-to-krita.md)); today it names files by position.
- A driven piece or drawing missing after a re-export is reported, as rig pieces are today, and that part of the stop is skipped, never silently deleted.
- Mirror makes a twin dial for mirrored pieces (left eye blink makes right eye blink).

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| Alt + drag | Smart dial | Turn freely, no snapping to stops |
| Ctrl + drag | Smart dial | Fine steps |
| Double click | Smart dial | Back to the rest stop |

## Open questions

- [ ] Should a dial also drive warp dots, or only moves and drawing swaps in the first version?
- [ ] Is snapping to stops the right default, or should free turning be the default with Alt to snap?

## Not in the first version

- Two-axis dials (a pad for up-down and left-right head turns at once). The pad UI exists for move, so this is the natural next step.
- Dials that drive other dials.
