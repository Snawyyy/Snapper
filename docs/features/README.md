# Features

Planned features for Snapper, one file per feature. The research behind them is in `docs/research/animation-tools.md`.

## Planned

In the order we plan to build them.

- [ ] [Smart Dials](smart-dials.md): one dial per action (head turn, blink, mouth) that swaps piece drawings and slides joints with them, keyed like any fader.
- [ ] [Smooth warp](smooth-warp.md): upscale drawings with xBR and blend when warping, so bent parts stay smooth instead of jagged.
- [ ] [Draw on a piece](draw-on-a-piece.md): draw a new drawing for a limb inside Snapper, with reference flicking, a floating palette and pixel tools.
- [ ] [Joint snap and joint circles](joint-snap.md): joints catch on the middle of the drawn limb's width, and a circle of that width fills the bend.
- [ ] [Transparent shot background](transparent-background.md): see-through shots over video in the reel, and export with alpha.
- [ ] [Skins](skins.md): a named set of drawings across many pieces that swaps a doll's whole look with one tile.
- [ ] [Doll to Krita](doll-to-krita.md): turn a doll folder back into a Krita file and round-trip it without losing the rig.
- [ ] [Objects](objects.md): PNG and GIF props a doll can hold, pass and drop, slotting into its draw order.
- [ ] [Editing outside the frame](off-frame-editing.md): limbs past the shot's frame are drawn dimmed and can be picked and edited, like a pasteboard.
- [ ] [Text and subtitles on the reel](reel-text-and-subtitles.md): text clips on Video tab tracks and a subtitle track timed by tapping to the song.
- [ ] [Compositing](compositing.md): a Comp tab with anime finishing effects that target a doll, pieces or palette colors.

## Adding a new feature

Copy the shape the files above use, so nothing is missed:

1. Title, a Status line (planned, building, done) and what inspired it.
2. One paragraph: what it is and what is wrong today, naming the code it touches.
3. Sections for how it is made, how it is used, keys and the timeline, selections and picking, undo, saving and Krita, speed, and edge cases, as far as they apply.
4. A Hotkeys table, checked against the keys Snapper already binds and against the table below.
5. Open questions for the owner, and what is not in the first version.
6. A line in the Planned list above, and its keys in the table below.

## All proposed hotkeys

Every proposed key is free in Snapper today (checked against the keys bound in the code), except where a row says it reuses one inside draw mode only. All of them go in the keys sheet and the find-a-command list.

| Key | Where | Does | Feature |
| --- | --- | --- | --- |
| Alt + drag | Smart dial | Turn freely, no snapping to stops | Smart Dials |
| Ctrl + drag | Smart dial | Fine steps | Smart Dials |
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
| A | Comp tab | Add an effect | Compositing |
| V (hold) | Comp tab | All effects off while held | Compositing |
| K (hold) | Comp tab | Show what the picked effect touches | Compositing |
