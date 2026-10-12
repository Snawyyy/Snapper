# Draw on a piece

Status: built, first pass (2026-10-12); owner tests by eye.

First pass: N (a blank drawing, the shown one kept for R and the onion), Shift+N (draw over the
drawing showing), the stage's right click and three "+ New drawing"
entries at the end of the Pose box's Drawing picker (blank, copy of
this one, copy of the default) start draw mode (`DrawManager`). It
covers the stage with the piece at its rest place (turned as the rig
rests it, so it lines up with the face), the rest of the doll dimmed
at rest, a frame at the drawing size that turns red while a stroke
runs past it, and pixel lines past 800 %; the wheel zooms, the middle
button pans, Shift+1 fits. Tools: hard pixel brush (B, [ and ], right
button erases, Shift+click draws a line), eraser (E, again for the brush), exact-color fill
(G), color picker (I, or Alt held); hold R for the original, D for
the onion at 30 %. Ctrl+Z and Ctrl+Y undo strokes inside draw mode. The
floating palette opens on the piece's colors and gives the brush color.
A name field in the title bar (default "mouth 2"); Done (Enter) saves
`snapper/<name>.png` in the doll folder, lists it in rig.json
(`RigPiece::own_drawings`, shown after the Krita drawings by
`AddOwnDrawings`) and shows it on the piece at that frame, as one undo
step; Edit on a Snapper drawing saves a new version in its place, on a
Krita drawing makes a copy. Cancel (Escape) asks in the bar when
something was drawn. The Krita exporter now carries the `snapper`
folder over like rig.json. Not built yet: the magic wand and
selections (H, arrows, Delete), the transparent pixels view (X), fill
tolerance, Fill all and Close gaps, the reference picker (the reference
is the drawing started from, or for a blank start the one showing),
the pencil badge and right click menu in the picker (Rename,
Duplicate, Delete, Show in folder), the Smart Dial "New drawing for
this stop", the Bend mesh question on Done (the mesh is kept; parts
outside it do not bend), the mirrored twin copy, and colliders or
Smooth warp redone for the new drawing. A Krita re-export
that adds drawings to a piece shifts the index of its Snapper drawings,
as reordering in Krita already does for keys.

Needs: the [floating palette](palette.md).

Draw a new drawing for a piece without leaving Snapper: a new mouth shape, a fist, a squinting eye. It becomes one more drawing of that piece, so keys, the Drawing picker and Smart Dial stops can use it like any drawing from Krita.

## Getting in

- In the Pose box's Drawing picker, a "+" tile: New drawing. It offers three starts: Blank, Copy of the drawing showing now, Copy of the default drawing. Copy is the usual start: change the mouth, keep the outline.
- Right click a piece on the stage: Draw new drawing, or Edit drawing on a drawing made in Snapper.
- Hotkey on a picked piece (proposed: N for new, Shift+N to edit the showing drawing).
- From a Smart Dial: on a stop, "New drawing for this stop" opens draw mode and sets the stop to the result when done.
- Krita drawings are never painted over in place, because the next Krita export would overwrite them. Edit drawing on one makes a copy and says so in the title.

## The draw mode

- The stage switches to the piece at rest: upright, unwarped, at its rig place, zoomed to fit. The rest of the doll stays visible, dimmed, in its rest pose, so a new mouth is drawn in its face.
- A thin frame shows the piece's drawing size. All drawings of a piece share one size, so strokes outside the frame are cut off and the frame says so while you draw there.
- Pixel grid lines appear past 800% zoom. Zoom and pan the same as the stage (wheel, middle drag, fit key).
- Physics, playback and posing pause while drawing. The title bar reads "Drawing: mouth O (head)" with Done and Cancel buttons.

## Reference

- Hold R: the original drawing shows in place of yours; let go and yours is back. Tap it repeatedly to flick between the two and see what moved.
- Original as onion: a toggle (D) keeps the original under yours at 30%, for tracing.
- "Original" means the drawing you started from; a picker beside it can choose any other drawing of the piece as the reference.

## Tools

- Brush (B): hard pixel brush, no soft edges, so it matches the pixel brush in Krita. [ and ] change size, size shown as a pixel outline at the cursor. Shift click draws a straight line from the last point.
- Eraser (E): the same brush, clearing to transparent. Right mouse button with the brush erases too, so switching is rare.
- Fill bucket (G): fills the touching area of the same color. Exact color by default since dolls use flat colors; a tolerance fader for the rare soft area. Option Fill all of this color, and a Close gaps option that treats gaps of 1 to 3 pixels in an outline as closed.
- Magic wand (W): picks the touching area of one color. Shift adds, Ctrl flips, the same as picking anywhere in Snapper. While a selection exists, brush, eraser and fill only touch inside it, and Delete clears it. Arrows nudge the picked pixels by one, Shift+arrows by ten; H flips them.
- Color picker (I): click to take a color. Holding Alt with any tool picks temporarily, then returns to the tool.
- Transparent pixels (proposed: X, a toggle): fully clear pixels show as a bold color fill, and half-clear pixels (stray alpha from soft brushes) show in a warning color, as in OpenToonz, so holes and dirt in a fill are found at a glance.
- Tool tiles in a small bar beside the stage, each with its key shown on it.

## Floating palette

Draw mode opens the [floating palette](palette.md) on the piece's own colors: every color in its drawings, biggest areas first, with the doll's other colors one tab away. Hovering a swatch flashes where it is used; right click adds a new color for this doll.

## Undo inside draw mode

- Ctrl+Z and Ctrl+Y undo and redo single strokes, fills and selections while drawing.
- Done (Enter) saves the drawing and adds one step to the project's undo, named ("Undo New drawing mouth O"). Undoing it removes the drawing; redo brings it back.
- Cancel (Escape) leaves without saving. If anything was drawn, it asks in place first (Discard or Keep drawing), never in a popup window.

## Saving next to Krita

- The drawing is saved as a PNG in a Snapper folder inside the doll's folder, and listed in the doll's rig file, which Snapper owns. Krita's own files are never written, so a re-export never deletes a Snapper drawing.
- Named in place when Done is pressed (default: "piece 2", "piece 3"). Renamable later from the Drawing picker.
- It goes into the project's copy of the doll at once, and Save rig writes it to the library, the same as any rig change.
- In the Drawing picker, Snapper drawings carry a small pencil badge, with right click: Edit, Rename, Duplicate, Delete, Show in folder.
- Delete is greyed while keys or dial stops use the drawing, with a tooltip naming the shots that use it.

## Edge cases

- Bend mesh: a piece with Bend ticked builds its mesh from all its drawings. If the new drawing stays inside the current mesh, nothing changes. If it goes outside, Done asks: Rebuild mesh (warp keys on this piece reset, with the count of keys shown) or Keep mesh (the parts outside do not bend).
- Colliders and outlines that come from the drawing are built again for the new drawing.
- Smooth warp upscales the new drawing again on save.
- The 64 drawings per piece limit: the "+" tile greys out at 64, with a tooltip saying why.
- Mirror: a mirrored twin piece gets the drawing flipped as its own copy, offered on Done.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| N | Piece picked | New drawing |
| Shift+N | Piece picked | Edit the showing drawing (a copy if it came from Krita) |
| R (hold) | Draw mode | Show the original drawing while held |
| D | Draw mode | Original as onion under yours, on and off |
| B | Draw mode | Brush |
| E | Draw mode | Eraser on and off (back to the brush) |
| G | Draw mode | Fill bucket |
| W | Draw mode | Magic wand |
| I | Draw mode | Color picker |
| Alt (hold) | Draw mode | Color picker while held |
| X | Draw mode | Show transparent pixels, on and off |
| [ and ] | Draw mode | Brush size down and up |
| H | Draw mode | Flip the picked pixels |
| Arrows, Shift+arrows | Draw mode | Nudge picked pixels by 1 or 10 (reused from the stage nudge) |
| Delete | Draw mode | Clear the picked pixels (reused from Delete on the stage) |
| Ctrl+Z, Ctrl+Y | Draw mode | Undo and redo one stroke |
| Enter | Draw mode | Done, save the drawing |
| Escape | Draw mode | Cancel, asks first if anything was drawn |

## Open questions

- [ ] Draw on the doll at rest (planned), or also on the doll as posed at the playhead? First pass: at rest only.
- [ ] When a new drawing goes past the bend mesh, is asking on Done right, or should it warn while drawing? First pass: neither; Done keeps the mesh.

## Not in the first version

- Growing a piece's drawing size, layers inside a drawing, soft or pressure brushes.
- Sending Snapper drawings back to Krita as layers (covered by [Doll to Krita](doll-to-krita.md)).
