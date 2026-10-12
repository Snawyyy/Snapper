# Features

Planned features for Snapper, one file per feature. The research behind them is in `docs/research/animation-tools.md`.

## Planned

In build order. Each file has a Needs line naming what must come first; Doll to Krita starts the list because its first part, the Krita exporter keeping layer names, is what Smart Dials and Skins stand on.

### Foundations: what other features stand on, and the bug-like gap

- [ ] [Doll to Krita](doll-to-krita.md): turn a doll folder back into a Krita file and round-trip it without losing the rig.
- [ ] [Floating palette](palette.md): one color panel offering the doll's own colors first, shared by drawing, joint circles, skins, compositing and text.
- [ ] [Editing outside the frame](off-frame-editing.md): limbs past the shot's frame are drawn dimmed and can be picked and edited, like a pasteboard.

### Doll tools

- [ ] [Smart Dials](smart-dials.md): one dial per action (head turn, blink, mouth) that swaps piece drawings and slides joints with them, keyed like any fader.
- [ ] [Draw on a piece](draw-on-a-piece.md): draw a new drawing for a limb inside Snapper, with reference flicking, a floating palette and pixel tools.
- [ ] [Joint snap and joint circles](joint-snap.md): joints catch on the middle of the drawn limb's width, and a circle of that width fills the bend.
- [ ] [Skins](skins.md): a named set of drawings across many pieces that swaps a doll's whole look with one tile.
- [ ] [Smooth warp](smooth-warp.md): upscale drawings with xBR and blend when warping, so bent parts stay smooth instead of jagged.
- [ ] [Objects](objects.md): PNG and GIF props a doll can hold, pass and drop, slotting into its draw order.

### Video tab and output

- [ ] [Resizing shots on the reel](reel-shot-resize.md): drag a shot clip's edge on the Video tab timeline to change the shot's own length.
- [ ] [Transparent shot background](transparent-background.md): see-through shots over video in the reel, and export with alpha.
- [ ] [Sound on the reel](reel-audio.md): audio tracks for sound effects, voice and music, with volume keys and fades; shots play the reel sound under them in the Pose tab.
- [ ] [Text and subtitles on the reel](reel-text-and-subtitles.md): text clips on Video tab tracks and a subtitle track timed by tapping to the song.
- [ ] [Reference on the Pose tab](pose-reference.md): a video, GIF or picture behind the doll with an opacity fader, in step with the timeline, never in the final video.

### Finishing

- [ ] [Compositing](compositing.md): a Comp tab with anime finishing effects, rim light included, that target a doll, pieces or palette colors.

## Adding a new feature

Copy the shape the files above use, so nothing is missed:

1. Title, a Status line (planned, building, done) and what inspired it, then a Needs line naming what must be built first.
2. One paragraph: what it is and what is wrong today, naming the code it touches.
3. Sections for how it is made, how it is used, keys and the timeline, selections and picking, undo, saving and Krita, speed, and edge cases, as far as they apply.
4. A Hotkeys table, checked against the keys Snapper already binds and against the table below.
5. Open questions for the owner, and what is not in the first version.
6. A line in the Planned list above, in its build-order group, and its keys in the table below.

## Modifier keys

Every plan uses the same meaning for each modifier, taken from what Snapper already does:

- Shift: fine steps while dragging or turning the wheel (faders, dials, the pad, stage drags, nudges by 10), and adds to the pick on a click. On the reel, where frames are already whole, Shift on a drag ripples, as Shift+Delete does.
- Ctrl: turns snapping off while dragging (stops, shape snaps, guides, cut markers). On a click it flips the pick; with the wheel it zooms.
- Alt: the drag's other action (the rig rope today; trim only, slide a video under its row, move a cue edge with its neighbour). Held alone in draw mode it is the color picker.

## All proposed hotkeys

Every proposed key is free in Snapper today (checked against the keys bound in the code), except where a row says it reuses one inside draw mode only. All of them go in the keys sheet and the find-a-command list.

| Key | Where | Does | Feature |
| --- | --- | --- | --- |
| Ctrl + drag | Smart dial | Turn freely, no snapping to stops | Smart Dials |
| Shift + drag or wheel | Smart dial | Fine steps | Smart Dials |
| Double click | Smart dial | Back to the rest stop | Smart Dials |
| Shift+[ and Shift+] | Doll picked | Previous and next skin | Skins |
| Backslash (hold) | Stage | Show Crisp while held, to compare with Smooth | Smooth warp |
| Shift+V (hold) | Stage, transparent shot | Preview over a solid color while held | Transparent background |
| Ctrl + drag | Rig canvas, joint | Place freely, no shape snapping | Joint snap |
| J | Rig canvas, joint picked | Joint circle on and off | Joint snap |
| N | Piece picked | New drawing | Draw on a piece |
| Shift+N | Piece picked | Edit the showing drawing (a copy if it came from Krita) | Draw on a piece |
| R (hold) | Draw mode | Show the original drawing while held | Draw on a piece |
| Shift+R | Draw mode | Original as onion under yours, on and off | Draw on a piece |
| B | Draw mode | Brush | Draw on a piece |
| E | Draw mode | Eraser | Draw on a piece |
| G | Draw mode | Fill bucket | Draw on a piece |
| W | Draw mode | Magic wand | Draw on a piece |
| I | Draw mode | Color picker | Draw on a piece |
| Alt (hold) | Draw mode | Color picker while held | Draw on a piece |
| X | Draw mode | Show transparent pixels, on and off | Draw on a piece |
| [ and ] | Draw mode | Brush size down and up | Draw on a piece |
| H | Draw mode | Flip the picked pixels | Draw on a piece |
| Arrows, Shift+arrows | Draw mode | Nudge picked pixels by 1 or 10 (reused from the stage nudge) | Draw on a piece |
| Delete | Draw mode | Clear the picked pixels (reused from Delete on the stage) | Draw on a piece |
| Ctrl+Z, Ctrl+Y | Draw mode | Undo and redo one stroke | Draw on a piece |
| Enter | Draw mode | Done, save the drawing | Draw on a piece |
| Escape | Draw mode | Cancel, asks first if anything was drawn | Draw on a piece |
| O | Stage | Show or hide the outside of the frame | Editing outside the frame |
| Ctrl+T | Video tab | Add a text clip at the playhead | Text and subtitles |
| L | Video tab, playing | Start the next subtitle line at the playhead | Text and subtitles |
| Ctrl+Enter | Editing text | Finish the edit | Text and subtitles |
| Alt + drag | Cue edge | Move it together with the next cue's start | Text and subtitles |
| Alt + edge drag | Shot clip | The other action: trim only on a full clip, resize the shot on a partial clip | Resizing shots |
| Shift + edge drag | Shot clip | Ripple: later clips on the track move by the change | Resizing shots |
| Ctrl + edge drag | Shot clip | No snapping | Resizing shots |
| Alt + drop | Pose tab | Add the dropped file as a reference | Reference |
| Y | Pose tab | Show or hide every reference | Reference |
| Alt + drag | Reference row | Slide the video under the row | Reference |
| M | Reference row picked | Drop a marker on the reference (reuses Mark cut) | Reference |
| A | Comp tab | Add an effect | Compositing |
| V (hold) | Comp tab | All effects off while held | Compositing |
| K (hold) | Comp tab | Show what the picked effect touches | Compositing |
| Shift+= and Shift+- | Layer or effect picked | Next and previous blend mode | Compositing |
