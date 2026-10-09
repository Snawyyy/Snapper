# Snapper UI ideas

Checked against CLAUDE.md "UX language" and src/ui. Already there and
skipped: one undo step per drag, Escape cancels, named undo/redo, greyed
controls with a reason, orange pick outline, yellow handles, Shift-drag
axis lock on the stage, box pick with Shift/Ctrl, Ctrl+A, Delete, reel
snapping to cuts and playhead, timeline Ctrl+wheel zoom, status bar
messages. Not found: stage hover highlight, stage zoom/pan, arrow nudge
on the stage, scrub labels, guides, command palette, any animation or
sound in the UI.

Cost: small = under a day, medium = a few days, large = a week or more.

## List 1: Figma

Ranked by value for cost, best first.

1. Hover highlight before click: the thing under the cursor gets a thin
   outline before you press, so you know what a click will grab.
   Where: stage doll/piece pick, warp dots, rig canvas pieces. Cost: small.

2. Scrub a number by dragging its label: hover the label, cursor turns
   into a left/right arrow, drag right to raise, Shift for coarse, Alt
   for fine; the whole drag is one undo step.
   Where: every inspector number field (layer box, camera, drag node Lag
   and Bounce, link strength, rig rest angle). Cost: small.

3. Arrow nudge: arrows move the pick 1 px, Shift+arrows 10 px (Figma's
   defaults); with a rotate pick, arrows turn 1 deg / 15 deg.
   Where: stage pick (when the stage has focus; the timeline keeps arrows
   for frame stepping). Cost: small.

4. Shift snaps rotation to 15 deg steps while dragging or wheeling.
   Where: stage wheel rotate and rotate handle drags. Cost: small.

5. Zoom to selection / zoom to fit: Shift+1 fits the whole stage,
   Shift+2 frames the pick, Shift+0 is 100%.
   Where: stage view (needs stage zoom/pan first, see 9). Cost: medium.

6. Live value label next to the cursor during a drag: shows the angle,
   offset or scale as it changes ("34 deg", "+12, -4").
   Where: stage drag and wheel rotate, warp dot drag, timeline key drag
   (shows "+3 f"). Cost: small.

7. Math in number fields: type "*2", "+15", "90/2" and it works out the
   value. Where: inspector number fields, shot length, export range.
   Cost: small.

8. "Mixed" for multi-pick: when picked pieces differ, the field shows
   "Mixed"; typing a value sets all, "Mixed+10" shifts all by 10.
   Where: inspector fields with several pieces or layers picked.
   Cost: medium.

9. Space+drag pans, Ctrl+wheel zooms toward the cursor, middle drag pans.
   Where: stage view and rig canvas. Cost: medium.

10. Smart guides: while moving a piece or doll, red lines appear when its
    centre or edge lines up with another doll, the stage centre or the
    frame edge, and it snaps to them within a few screen pixels.
    Where: stage doll move, rig canvas pivot placement. Cost: medium.

11. Distance labels on Alt hover: with something picked, hold Alt and
    hover another thing to see the gap in px between them.
    Where: stage. Cost: medium.

12. Layer panel hover links to canvas: hovering a row in a list outlines
    that thing on the stage, and the reverse.
    Where: cast panel rows, timeline doll row headers, rig piece list.
    Cost: small.

13. Quick actions / command palette: Ctrl+/ opens a search box over every
    menu action by name, with its shortcut shown, greyed actions explain
    why. Where: main window, built from the existing QActions.
    Cost: medium.

14. Context property panel: the inspector shows only what fits the pick
    (piece, doll, camera, effect, warp dot) and the title says what is
    picked ("Head, frame 24"). Where: inspector. Cost: small if the boxes
    already swap; mostly a title line.

15. Toast with Undo: after a big action (delete keys, paste pose, mirror)
    a short note at the bottom says what happened with an Undo button,
    gone after ~4 s. Where: main window status area. Cost: small.

16. Empty states that teach: an empty stage, timeline or reel shows one
    line and one button ("Drop a doll here, or open the Cast").
    Where: stage, timeline, reel, cast panel. Cost: small.

17. Selection bounds with handles: a box round the picked doll with
    corner handles to scale and a rotate zone just outside the corners.
    Where: stage pick. Cost: medium.

18. Double click to dive in: double click a doll to pick a piece inside
    it, Escape climbs back out (Figma's enter/exit group).
    Where: stage pick (whole doll vs piece). Cost: small.

19. Keyboard shortcuts sheet: Ctrl+Shift+? opens a panel listing every
    key, drawn on a keyboard. Where: Help menu. Cost: small.

20. Tab / Shift+Tab steps the pick to the next or previous piece; Enter
    picks the children, Shift+Enter the parent.
    Where: stage and rig canvas pick. Cost: small.

21. Rename in place: double click a name in a list to edit it there.
    Where: cast panel, shot bin, rig piece list. Cost: small.

## List 2: Game feel / juice for tool UI

Ranked by value for cost, best first.

1. Instant feedback budget: something on screen changes on the same
   frame as the press; never let input to feedback pass 100 ms (Swink's
   real-time limit), aim under 50 ms. Where: all stage, timeline and rig
   drags; profile the repaint on drag move. Cost: small to check,
   medium if a slow path turns up.

2. Grab big, look small: hit area 2 to 3 times the drawn size, nearest
   handle wins when they overlap, a press a few px off still grabs.
   Where: warp dots, pivots, timeline keys, cut markers, reel clip edges.
   Cost: small (UX language already says it; audit every handle).

3. Drag dead zone and late grab: a press does not start a drag until the
   mouse moves ~3 to 4 px, so a click never nudges; once moving, the
   thing jumps to the cursor with no lag. Where: stage pick, timeline
   key drag, reel clips. Cost: small.

4. Snap feedback: when a drag snaps (to a frame, a cut, a guide), the
   target flashes bright for ~100 ms and the dragged thing gives a 1 to
   2 px settle. Where: reel snapping, timeline key drag frame snaps,
   stage guides. Cost: small.

5. Press and release states on custom controls: down = 1 px sunken,
   darker (fits Win98 bevels); release pops back in ~80 ms. Tap shows
   at least one frame of the down state even on a fast click.
   Where: timeline corner mode toggle, transport buttons, toolbar
   buttons, reel clip press. Cost: small.

6. Hover anticipation: hover brightens a handle and grows it ~15 % over
   80 to 120 ms ease-out, cursor changes to say what a drag will do
   (rotate, move, scrub). Where: stage handles, warp dots, timeline keys.
   Cost: small.

7. Key placed pop: a new key scales from 1.4 to 1.0 with ease-out-back
   over ~150 ms and a quick Teto red flash ring.
   Where: timeline key creation (posing at a new frame). Cost: small.

8. Easing everywhere it moves by itself: anything the app moves (zoom to
   fit, timeline scroll to playhead, panels opening) uses ease-out
   cubic in 150 to 250 ms, never linear and never over 300 ms.
   Where: timeline auto scroll, stage zoom, panel open. Cost: small.

9. Delete puff: removed keys or dots shrink to 0 over ~120 ms with a few
   small pixels flying off, so the change is seen, not just gone.
   Where: timeline key Delete, warp dot remove, reel clip delete.
   Cost: small.

10. Tiny hit-stop on big confirms: hold the confirm frame ~60 to 80 ms
    (Nijman's "sleep") before the follow-on animation, so the action
    lands. Where: Paste pose, Mirror, Export finished. Cost: small.

11. Undo rewind cue: on undo, the changed thing flashes and slides back
    over ~120 ms instead of snapping, so you see what was undone.
    Where: stage after undo/redo, timeline keys. Cost: medium.

12. Optional UI sounds: short dry clicks (under 80 ms, fast attack),
    random pitch +/-4 % so repeats do not drone, rising pitch for
    stacked snaps, quiet by default and off with one toggle; mute while
    playback runs. Where: key place, snap, delete, export done.
    Cost: medium.

13. Squash on drop: a dragged doll or clip squashes ~5 % on release and
    springs back (cubic-bezier 0.34, 1.56, 0.64, 1, ~200 ms).
    Where: reel clip drop, cast-to-stage doll drop. Cost: small.

14. Input buffering: a key press during a running animation or a busy
    repaint is queued and applied, not dropped (~120 ms window).
    Where: timeline frame stepping, Delete during a drag end.
    Cost: small.

15. Coyote time for handles: right after a handle hides (hover left,
    pick changed), it stays grabbable for ~100 ms. Where: stage handles,
    warp dots. Cost: small.

16. Success burst: a ring of Teto red pixels and a short screen flash on
    finished long jobs. Where: export dialog done, project saved after
    a long autosave. Cost: small.

17. Permanence: leave a faint ghost of where a dragged thing started
    until the drag ends, and fade trails of the last few positions.
    Where: stage drag (start ghost), timeline key drag (old slot).
    Cost: small.

18. Error shake: a blocked action shakes the control 3 to 4 px for
    ~200 ms and shows the reason, instead of silently doing nothing.
    Where: greyed buttons clicked anyway, invalid field input.
    Cost: small.

19. Playhead weight: the playhead snaps to frames with a tiny overshoot
    when released, and the frame number ticks up with a short bump.
    Where: timeline and reel playhead scrub. Cost: small.

20. Consistency table: one set of durations and curves (press 80 ms,
    hover 100 ms, pop 150 ms, move 200 ms) held in one place so every
    control feels like the same app. Where: theme module (theme.h).
    Cost: small.

21. Off switch: one "Motion and sound" setting (Full, Calm, Off); every
    effect asks it before playing, and the app works the same with all
    of it off. Where: View or settings menu. Cost: small, do it first
    with item 20.

22. Keep it short and rare: no effect over 300 ms on an action done
    hundreds of times; big effects only for rare events (export,
    first doll on stage); no shake on routine edits.
    Where: review rule for every item above. Cost: none.

## Sources

- Figma Help, Adjust alignment, rotation, position and dimensions:
  https://help.figma.com/hc/en-us/articles/360039956914
- Figma Help, Measure distances between layers:
  https://help.figma.com/hc/en-us/articles/360039956974
- Supademo, How to adjust the nudge amount in Figma:
  https://supademo.com/blog/guides/how-to-adjust-the-nudge-amount-in-figma/
- Figma shortcuts (Shift+1, Shift+2, Ctrl+/):
  https://figmaplugins.co/guides/figma-zoom-shortcuts/
- Steve Swink, Game Feel (100 ms real-time response):
  https://en.wikipedia.org/wiki/Game_feel
- Jan Willem Nijman, The art of screenshake (INDIGO 2013):
  https://www.youtube.com/watch?v=AJdEqssNZ-U
- Blue Tengu, experiments on Nijman's talk:
  https://www.bluetengu.com/2014/12/12/art-of-screenshake-experiments/
- Martin Jonasson and Petri Purho, Juice it or lose it (GDC Europe 2012):
  https://github.com/jimfingal/love-juicy
- Game feel on the web (hitstop 60 to 90 ms, pitch +/-4 %, reduced
  motion): https://valdemird.com/blog/game-feel-on-the-web/
- Coyote time and input buffering (80 to 150 ms):
  https://www.gamejuice.co.uk/articles/coyote-time-input-buffering
- Button press timing (80 to 150 ms, ease-out-back):
  https://artofstyleframe.com/blog/micro-interactions-ui-when-to-animate/
