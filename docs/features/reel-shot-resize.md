# Resizing shots on the reel

Status: built, first pass (2026-10-12); owner tests by eye. Asked for by the owner for the Video tab.

First pass: dragging either edge of a full shot clip changes the shot itself (`ReelManager::ResizeShots`, `edit/reel_resize.cpp`); Alt trims instead, and on a partial clip Alt resizes. The left edge adds or takes frames at the shot's start and moves every key, link, hold and layer span with it (`ShiftShotFront`, `edit/shot_timing.cpp`). The Edges button in the reel bar (and the clip's right click menu) switches Hold and Stretch (`StretchShot`). Every full clip of the shot follows, later clips ripple (Shift keeps them put), a clip in the way stops the drag and Ctrl turns snapping off. The chip shows "72 f, 3.0 s  +24" and what stopped it. Many picked shot clips resize together, as one undo step named "Resize shot Intro"; Escape cancels; the playhead stays put. A clip showing its shot's end says "3 keys hidden" when keys sit past the end. Double clicking a shot clip inside a gap offers Fit shot to gap, held or stretched.

Not built yet: the fit offer on a drop from the shot strip; greyed hidden keys on the Pose tab timeline; a separate right click menu for a clip edge (the edge mode is on the clip menu) and saving the edge mode.

Defaults taken for the open questions and gaps: resizing is the default on full clips (Alt trims); keys past the end are kept hidden. Alt is read at the press, not switched mid drag. Taking frames off the start folds the keys before the new start onto frame 0 (the later kept), so those are not kept hidden. Every other full clip of the shot grows too and can block the drag like the dragged clip's neighbour.

Needs: nothing.

On the reel a shot clip is a link to a shot, and trimming its edges only changes which part of the shot plays (`in` and `length` in `model/reel.h`). The shot's own length (`Shot::length`, two seconds by default) can only be changed on the Pose tab. Cutting to music happens on the reel, so the reel is where a shot should be made longer or shorter: dragging a shot clip's edge changes the shot itself.

## Dragging an edge

- A shot clip that plays the whole shot (from its first frame to its last) is a full clip. Dragging a full clip's right edge changes the shot's length through `ShotManager::SetLength`; the clip grows or shrinks with it and stays full.
- Dragging the left edge of a full clip adds or removes frames at the shot's start: the shot gets longer or shorter at the front, its keys move with its content, and the clip's start moves on the reel.
- A clip that plays only part of a shot is trimmed as today.
- Alt held during the drag does the other one: on a full clip it only trims the clip, leaving the shot as it is; on a partial clip it resizes the shot instead of trimming.
- Ctrl held turns snapping off, as on every drag.
- Snapping as every reel drag: cut markers, other clips' cuts, the playhead, frame 0. A shot can be sized to land exactly on the next cut marker.
- While dragging, a chip shows the new length in frames and seconds ("72 f, 3.0 s") and the change ("+24").

## Keys when the length changes

- Longer: new frames hold the last pose, as keys do everywhere.
- Shorter: keys past the new end are kept, not deleted, and the clip shows a small "3 keys hidden" badge at its end. Making the shot long again brings them back. The Pose tab timeline shows them greyed past the end.
- Stretch: an Edge mode switch in the reel bar (Hold, the default, or Stretch), also on the right click menu of a clip edge, makes the drag stretch the shot's timing instead, moving every key in proportion so the same action plays faster or slower. Keys land on whole frames; two keys that would land on one frame keep the later.
- Physics (jelly, cloth, drag dots) simply runs over the new length.

## Other clips of the same shot

- Every other full clip of the shot follows the new length, since they all show the whole shot.
- Partial clips keep their in point and length; a clip that now runs past the shot's new end shows its tail hatched, as today.
- Clips after the resized or trimmed one on the same track move by the change, so touching cuts stay touching and transitions on those cuts stay. Holding Shift keeps them put.
- If the clip cannot grow because the next clip on the track is in the way, it stops at that clip and the chip says why; the ripple drag goes past it.

## Fitting a shot to a gap

- Double clicking a shot clip inside a gap between cut markers offers Fit shot to gap: the shot's length becomes the gap's length (held or stretched, picked in the same box), one undo step.
- Dragging a shot from the shot strip onto the reel inside a gap offers the same fit on drop.

## Picking many

- With several shot clips picked, dragging the edge of one resizes all of them by the same number of frames, as one undo step, the same as `ShotManager`'s many-shot length change today.

## Undo and the rest of Snapper

- The whole drag is one undo step, named ("Undo Resize shot Intro"), covering the shot's length and every clip that moved. Escape during the drag cancels it.
- The Pose tab's master track and the shot strip show the new length at once.
- The playhead stays on the same reel frame during and after the drag.

## Edge cases

- Lengths stay between 1 frame and the shot length limit; the drag stops at either end with the chip saying why.
- A shot used on the reel and also played on its own (no reel clips yet) behaves the same, since the length lives in the shot.
- Shortening a shot below a key a Smart Dial or skin change sits on: that key is kept hidden like any other.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| Alt + edge drag | Shot clip | The other action: trim only on a full clip, resize the shot on a partial clip |
| Edge drag | Any clip | Ripple: later clips on the track move by the change; Shift keeps them put |
| Ctrl + edge drag | Shot clip | No snapping |

## Open questions

- [ ] Is resizing the shot the right default for full clips, with Alt to trim only, or the other way round?
- [ ] When shortening, keep keys hidden past the end (planned) or delete them?

## Not in the first version

- Speed changes on video clips (slow motion, speed up).
