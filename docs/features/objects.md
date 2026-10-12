# Objects

Status: planned.

Props such as a cup or a mic, from any PNG or GIF. Snapper already has a picture layer (`ImageLayer` in `model/layer.h`), but it is one still image with a path and nothing else: it cannot be held, animate, bend or sit between a doll's fingers. An object fixes that by being a doll with one piece, so it gets warp, jelly, smooth warp, joint snap and skins for free.

## Adding objects

- An Objects row in the Cast panel beside the dolls, as cards. Drop PNG, GIF, animated PNG or WebP files on it, or on the stage straight away.
- Dropping on the stage adds it to the library and places it where it was dropped, at its own pixel size scaled to the canvas the same way dolls are.
- Several files dropped at once: one object each. A folder dropped: every picture in it.
- Name from the file name, renamable in place. Right click a card: Rename, Replace picture, Show in folder, Delete (greyed while shots use it, naming them).
- Objects are copied into the project like dolls, so the project opens the same if the original file moves.

## GIFs and animated pictures

- Each frame becomes a drawing of the object's one piece, so everything that swaps drawings works on it (keys, Smart Dials, skins).
- Play mode in the object's settings: Loop (default), Once, Ping-pong, or Hold (stays on the keyed drawing, for picking a frame by hand).
- Speed: the GIF's own timing by default, or a set frames-per-drawing on twos and threes to match the shot.
- Start frame and an offset, so two copies of the same flame do not flicker in step.
- Played frames follow the shot clock, so scrubbing shows the exact frame and export matches preview.

## Holding an object

- Hold: pick the object, then right click a doll piece and choose Hold with this (or drag the object onto a hand with Alt, like the rig rope). From that frame the object follows the piece fully: position, turn and scale, not only x and y as links do today.
- Grip point: the object's pivot is where the hand holds it, set by dragging the pivot, with [joint snap](joint-snap.md) catching on the middle of the handle's width.
- Draw order inside the doll: a held object slots between the doll's pieces, set by dragging its row among the doll's pieces (the cup in front of the fingers, behind the palm). Saved per hold.
- Its own keys still work on top, so a held cup can tilt in the hand.
- Let go: right click and Drop at a frame. It stays where it was in the shot and keeps its keys from there.
- Hand-off: Hold with another piece at a later frame (a throw caught by a second doll). The hold changes are keys on an object row in the timeline, shown as chips naming the holder.
- Changing holder never jumps: the object stays where it is at that frame and follows from there, the same rule links use.

## Everything dolls have

- Warp and bend: tick Bend on the object to give it a mesh and dots, so a mic cable can sway as jelly or a paper bag can crumple.
- [Smooth warp](smooth-warp.md), joint circles and [drawing on a piece](draw-on-a-piece.md) (a new drawing for a dented cup) work the same.
- Effects targets: an object can be the target of a [Comp](compositing.md) effect, like a doll (a glow on a held lightsaber).
- Picking, multi-pick, groups, undo naming, copy and paste of settings all as for dolls.

## Undo and saving

- Every add, hold, drop and setting change is one undo step, named ("Undo Hold cup").
- Objects save in the library as a folder like a doll (the frames as PNGs plus their rig), so a held cup's grip and bend come with it into every project.
- [Doll to Krita](doll-to-krita.md) works on objects too.

## Edge cases

- A holder piece hidden by a skin: the object stays held and hidden with it, or stays shown, per a Hide with holder switch (on by default).
- A holder doll removed from the shot: the object drops at the frame it was removed, with a note on its row.
- Huge pictures (a 4000 px photo): offered a scale-down on import, keeping the original in the folder.
- A GIF with more than 64 frames: a piece holds at most 64 drawings, so the import offers to keep every Nth frame to fit, saying how many.

## Not in the first version

- Video files as objects (the reel handles video), and objects that break apart into pieces.
