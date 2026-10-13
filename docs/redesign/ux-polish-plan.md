# UX polish plan (2026-10-13)

The owner asked for a UI and UX polish pass: windows that fit any screen
size and shape, the same picking rules everywhere, every panel in the
Pose box style (raised title plates over sunken frames, golden ratio
splits where they fit, borders that show what belongs together, titles
outdented with their settings indented under them, controls that explain
themselves), no rounded corners, and only what the pick and tab need on
screen. The work follows the owner's workflow:

1. Sound on the Video tab timeline to plan timing, including recording
   sound (mouthing a scene's noises gives good pacing).
2. Make shots and put them on the timeline.
3. Sketch each shot on a draft layer (one drawable raster per shot) to
   match timing and plan it.
4. Adjust timing and shots, going back and forth between Video and shots.
5. Key and animate characters and objects, add backgrounds.
6. Composite each shot (Comp tab).
7. Render.

## Problems found

Window sizing and layout:
- `main_window.cpp`: fixed `resize(1440, 900)`; no saved geometry or dock
  state; dock widths only from minimums; no reaction to a new monitor.
- Splits are 3:1 (Pose, Rig) and 3:2, 1:4:1 (Video), none golden.
- `inspector.cpp` and `scroll_fit.cpp`: minimum width grows without a cap.
- `reel_timeline.cpp`: minimum height grows per track, no vertical
  scroll, reset on every frame change.
- `video_page.cpp`: the bar never wraps.
- `command_palette.cpp`, `shortcut_sheet.cpp`: fixed pixel sizes.
- `shot_strip_layout.h`: fixed strip height, ignores the font.

Selection consistency:
- Shift+Ctrl means different things: Shift wins in `pose_tool.cpp`,
  `rig_canvas_input.cpp`, `timeline_input.cpp`; Ctrl wins in
  `shot_strip.cpp`, `reel_timeline_input.cpp`, `group_chips.cpp`. The
  mapping is copied in about 8 places.
- `pose_tool.cpp`: Ctrl on a dot box toggles; elsewhere Ctrl+box removes.
- `cast_panel.cpp`, `doll_cards.cpp`: Qt ExtendedSelection (Shift picks a
  range); `shot_bin.cpp` single pick; subtitle cues Qt defaults.
- References pick one at a time, with no Shift, Ctrl, box or Ctrl+A, and
  leave the stage pick live beside them.
- Ctrl+A, Delete, Escape missing on Cast lists, doll cards, shot bin, cue
  list, reference rows, group chips, skins.
- Skins: Shift+click means stack (a wear action), against "Shift adds".

Missing functionality by workflow step:
1. No sound recording; no panel for a sound clip.
2. Shots made only in the Pose strip; no "New shot here" from a reel gap
   or marked range.
3. Draft layers do not exist.
4. Double click on a shot clip only offers Fit; no "open in Pose" and no
   key back to Video; tab order is Pose, Rig, Video.
5. Backgrounds only through Cast > Add > Picture.

Panel style: plain forms in shot, object, link, layer, reference, reel
text, export, rig panels; bare group boxes in dial, skin, poses, group,
collider, look, skin rig boxes; the fold header is a flat button with no
framed, indented body; `AddTitle` is a bold label; golden constants are
copied; the plate helpers are hidden in `pose_controls.cpp`; rounded
corners in `dial_track.cpp`, `drag_feedback.cpp`,
`reel_timeline_badges.cpp`, `stage_view_reference.cpp`,
`palette_panel.cpp`.

Clutter: the inspector shows Shot and Camera together, Dials and Skins
for dolls that have none, Saved poses always, References whatever is
picked; Cast shows on Rig and repeats the timeline rows on Pose; the
Video tab's Text and Subtitles tabs and "Edges" always show; the rig
panel stacks every section.

## Shared style kit (WP0, first)

- `ui/panel_kit`: `MakePlate`, `Framed`, `StackIn`, `InSlot` (moved out
  of `pose_controls.cpp`), `PlatedSection(title, body)` (outdented raised
  plate over a sunken frame), `SettingsGroup` (rows indented by
  `kIndent`), `GoldenRow(long, short)` with `kGoldenLong`,
  `kGoldenShort`, `GoldenSplit(total)` in one place, `TileRow`,
  `EmptyHint`.
- `ui/flow_layout`: a wrapping layout for button bars.
- `ui/screen_fit`: `FitWindowToScreen` (85 % of the screen, refit on a
  new screen), `GoldenDocks` (centre 0.618), `RememberLayout(window,
  key)` (refuses off-screen geometry), `FitDialog(widget, em)`, and a
  portrait rule (width/height under 1.2: Cast tabbed with Inspector).
- `ui/pick_keys`: `ClickMode(mods, is_picked)`: Ctrl toggles and wins
  over Shift, Shift adds, a plain click on something picked keeps the
  pick so it can be dragged; `BoxMode(mods)`: Shift adds, Ctrl removes.
- `ui/pick_list`: an event filter that makes any item view follow
  `ClickMode`, Ctrl+A, Delete (a signal) and Escape.
- Extend `fold_section` (plate header, square triangle, sunken framed
  body), `form_helpers` `AddTitle` (outdented plate), `theme` (`kIndent`,
  `kRowGap`, `kCellGap`, `Em`), `scroll_fit` (cap at 30 % of the
  screen), `snapper_style` (square sunken group frames), and fix the
  rounded corners.

## Work packages

- WP0 Style kit and pick rule: the files above. First.
- WP1 Window shell, tabs, dialogs: `main_window`, `top_bar`, tabs,
  `transport_bar`, `app/main.cpp`, dialogs. Screen fit, golden splits,
  remembered layout, Cast hidden on Rig, tab order Video, Pose, Rig,
  Comp, Export in the top bar, open a shot from the reel and a key back.
  After WP0 and the Comp tab.
- WP2 Video tab: sound recording, reel track scroll, wrapping bar, side
  panel only for the pick (text, sound clip, shot clip), New shot here,
  double click opens the shot, `PickList` and `ClickMode`. After WP0.
- WP3 Draft layer per shot: raster cels on a dashed Draft row, drawn
  with the draw mode brush, onion, opacity, eye; shown on Pose and the
  Video preview, left out of export by default. After Comp commits.
- WP4 Inspector in the Pose box style and tighter `ShowFor`. After WP0.
- WP5 Canvases, Cast, Rig panel and selection adoption: `ClickMode` and
  `BoxMode` everywhere, references in the pick rule, Cast as plated
  sections, Background quick add, Rig panel plated and showing only what
  the pick needs. After WP0.
