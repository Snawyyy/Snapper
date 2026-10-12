# Reference on the Pose tab

Status: planned. Asked for by the owner.

Animating from reference (a filmed take, a dance video, a GIF of a walk) means seeing it behind the doll frame by frame. Today a picture layer would show it, but it would also end up in the shot, the Video tab and the export. A reference is a video, GIF or picture placed in a shot's Pose tab only: it plays in step with the timeline, has an opacity slider, and never shows anywhere outside the Pose tab.

## Adding a reference

- Drop a video, GIF, animated PNG or still picture on the Pose tab's timeline or stage while holding Alt, or use Add reference in the shot's menu. Dropping without Alt still adds a normal layer, as today.
- It gets its own row at the top of the Pose timeline, marked with a reference icon and a dashed outline, so it never reads as part of the shot.
- Several references per shot are allowed (a front view and a side view), each with its own row.
- The file is linked, not copied into the project, since references are often big videos; a moved file shows a "find file" chip on its row.

## Seeing it

- Opacity: a fader on the reference's row and in its box, 50% by default; the wheel steps it like every fader.
- Behind or in front: a switch puts it behind every layer (tracing a pose) or over everything (checking a silhouette against the doll). Behind is the default.
- Place: move and scale it on the stage with handles like a picture; Fit to frame and Fit to height tiles; flip horizontally for mirrored reference. The frame of the video is shown with a thin dashed border.
- Hide: a key (proposed: Y) shows and hides every reference at once, for checking the animation alone; each row also has an eye.
- Tint: an optional single-color tint (blue or red, like a light table) so the reference never gets mistaken for the art.

## Time

- The reference plays frame for frame with the shot's playhead, so scrubbing shows the matching reference frame.
- Offset: drag its row bar left or right to line its start up with the shot; Alt drag slides the video under the row instead (picking which part plays), the same idea as the Video tab's slot picker.
- Speed: 100% by default; a fader slows it down or speeds it up, and a Match tile sets the speed so a marked stretch of the reference fits a marked stretch of the shot.
- Frame rate: a reference at 30 or 60 fps is sampled at the shot's frame rate, showing the nearest frame, never blended.
- Markers: M while the reference row is picked drops a marker on the reference (a foot down, a hit), shown as ticks on its row and as faint lines across the doll's rows, to key against.

## Never in the final video

- References are skipped by the renderer for export, the Video tab, shot thumbnails, the reel, Doll to Krita and every other picture Snapper makes; only the Pose tab stage draws them.
- They are not keys and not layers of the shot, so copying, pasting, duplicating a shot and undo of posing never touch them by accident. Duplicating a shot offers to keep its references.
- Clicking on the stage picks dolls through a reference behind them; a reference in front is click-through unless its row is picked.

## Undo and saving

- Adding, moving, offset, speed and opacity changes are one undo step each, named ("Undo Reference opacity").
- References save with the shot in the project file, by path, so the project opens with them in place; a missing file keeps its settings until found.

## Speed

- Video frames come through the existing video reader, which keeps the file open and decodes each frame once when playing forward, as the Video tab does.
- While scrubbing fast, the nearest decoded frame shows and the exact one follows when the playhead rests.

## Edge cases

- A reference longer than the shot: only the part under the shot plays; a shorter one holds its last frame, faded, with a "reference ended" chip.
- A GIF with its own timing: played at its own timing scaled to the shot's frame rate.
- The reference row is not counted in the shot's length and never changes it.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| Alt + drop | Pose tab | Add the dropped file as a reference |
| Y | Pose tab | Show or hide every reference |
| Alt + drag | Reference row | Slide the video under the row |
| M | Reference row picked | Drop a marker on the reference (reuses Mark cut while a reference row is picked) |

## Open questions

- [ ] Should references also be allowed on the Rig tab, for building a rig over a turnaround sheet?
- [ ] Default opacity 50%, or lower?

## Not in the first version

- Rotoscoping helpers (tracking a point in the video and making keys from it).
