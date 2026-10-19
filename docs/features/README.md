# Features

Planned features for Snapper, one file per feature. The research behind them is in `docs/research/animation-tools.md`.

## Planned

In build order. Each file has a Needs line naming what must come first. The old PNG dolls and the tools built on them (Krita export, smart dials, skins, joint snap, smooth warp, objects, drawing on a piece) are gone: characters are drawn and rigged in Snapper, Moho style (`docs/redesign/moho-plan.md`).

### Foundations: what other features stand on, and the bug-like gap

- [x] [Floating palette](palette.md): one color panel offering the picked character's colors first, shared by drawing, compositing and text.
- [x] [Editing outside the frame](off-frame-editing.md): what lies past the shot's frame is drawn dimmed and can be picked and edited, like a pasteboard.

### Characters

- [ ] [Moho style characters](../redesign/moho-plan.md): vector drawing, bones, smart bones and switch layers, drawn and rigged on the Character tab.

### Video tab and output

- [x] [Resizing shots on the reel](reel-shot-resize.md): drag a shot clip's edge on the Video tab timeline to change the shot's own length.
- [x] [Transparent shot background](transparent-background.md): see-through shots over video in the reel, and export with alpha.
- [x] [Sound on the reel](reel-audio.md): audio tracks for sound effects, voice and music, with volume keys and fades; shots play the reel sound under them in the Pose tab.
- [x] [Text and subtitles on the reel](reel-text-and-subtitles.md): text clips on Video tab tracks and a subtitle track timed by tapping to the song.
- [x] [Reference on the Pose tab](pose-reference.md): a video, GIF or picture behind the characters with an opacity fader, in step with the timeline, never in the final video.

### Finishing

- [x] [Compositing](compositing.md): a Comp tab with anime finishing effects, rim light included, that target a layer or palette colors.

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

- Shift: fine steps while dragging or turning the wheel (faders, dials, the pad, stage drags, nudges by 10), and adds to the pick on a click. On the reel, edge drags ripple by default and Shift keeps later clips put.
- Ctrl: turns snapping off while dragging (stops, shape snaps, guides, cut markers). On a click it flips the pick; with the wheel it zooms.
- Alt: the drag's other action (the rig rope today; trim only, slide a video under its row, move a cue edge with its neighbour). Held alone in draw mode it is the color picker.

## All proposed hotkeys

Every proposed key is free in Snapper today (checked against the keys bound in the code), except where a row says it reuses one inside draw mode only. All of them go in the keys sheet and the find-a-command list.

| Key | Where | Does | Feature |
| --- | --- | --- | --- |
| Shift+V (hold) | Stage, transparent shot | Preview over a solid color while held | Transparent background |
| O | Stage | Show or hide the outside of the frame | Editing outside the frame |
| Shift+3 | Stage | Fit all: the frame and everything in the shot | Editing outside the frame |
| Ctrl+T | Video tab | Add a text clip at the playhead | Text and subtitles |
| L | Video tab, playing | Start the next subtitle line at the playhead | Text and subtitles |
| Ctrl+Enter | Editing text | Finish the edit | Text and subtitles |
| Alt + drag | Cue edge | Move it together with the next cue's start | Text and subtitles |
| Alt + edge drag | Shot clip | The other action: trim only on a full clip, resize the shot on a partial clip | Resizing shots |
| Edge drag | Any clip | Ripple: later clips on the track move by the change; Shift keeps them put | Resizing shots |
| Ctrl + edge drag | Shot clip | No snapping | Resizing shots |
| Alt + drop | Pose tab | Add the dropped file as a reference | Reference |
| Y | Pose tab | Show or hide every reference | Reference |
| Alt + drag | Reference row | Slide the video under the row | Reference |
| M | Reference row picked | Drop a marker on the reference (reuses Mark cut) | Reference |
| A | Comp tab | Add an effect | Compositing |
| V (hold) | Comp tab | All effects off while held | Compositing |
| K (hold) | Comp tab | Show what the picked effect touches | Compositing |
| Shift+= and Shift+- | Layer or effect picked | Next and previous blend mode | Compositing |
