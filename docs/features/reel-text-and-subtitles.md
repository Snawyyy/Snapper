# Text and subtitles on the reel

Status: planned. Asked for by the owner for the Video tab.

The Video tab's reel holds video clips and shot clips only (`ClipSource` in `model/reel.h`). Text exists, but only inside a shot as a text layer for lyrics, so a title over a speedpaint, a caption across several shots or a full subtitle file needs a trip through another editor. This adds text clips that sit on reel tracks like any clip, and a subtitle track for timed lines.

## Text clips

- A text clip is a new clip source beside video and shot. It sits on any reel track, is drawn over the tracks below it, and moves, trims, splits, copies, snaps and ripple-deletes exactly like other clips, through `ReelManager`.
- Add: a "T+" tile in the reel's bar, or Ctrl+T, drops a text clip at the playhead on the highest empty track, 2 seconds long, and opens it for typing straight away.
- Look: the same settings as a shot's text layer (font, size, bold, fill, outline color and width), so a style learned in the Pose tab works here. Colors come from the floating palette.
- Place: drag the text on the video preview to move it; corner handles scale it. Position tiles snap it to the usual places (bottom center, top center, lower third left, middle). Guides catch on the frame center and a title-safe margin (10% in from each edge).
- Edit in place: double click the text on the preview or the clip to type; Escape cancels the edit, Enter on its own line adds a line, Ctrl+Enter finishes.
- In and out: tiles for how it appears and leaves (Cut, Fade, Pop, Slide up, Type on), with a length in frames. They are part of the clip, so trimming keeps them at the ends.
- Styles: save a look as a named style ("Title", "Caption", "Speaker"), apply it to many picked clips at once; a style can stay linked so changing it changes every clip using it.

## Subtitle track

- One subtitle track above all others, shown when the first subtitle is added. Each cue is a short text clip that cannot overlap the next.
- Cues share one style set on the track (font, size, colors, outline, a box behind the text, position), so a whole video's subtitles change in one place. A single cue can still override.
- Tap to time: paste the lines (lyrics or a script) into the subtitle box, play the song, and press L at the start of each line; each press ends the current cue and starts the next one at the playhead. Holding L while playing keeps the current line on until let go. The same way cut markers are marked by ear today.
- Cue starts and ends snap to cut markers, other clips' cuts and the playhead, and Alt drag moves a cue's edge together with the next cue's start, so gaps never open.
- A list view of all cues (time, text) beside the reel, editable as text; clicking a row jumps the playhead there. Find and replace across cues.

## Language and direction

- Right-to-left text (Hebrew, Arabic) lays out right to left, mixed with left-to-right words and numbers correctly, using Qt's text layout.
- Fonts fall back per character, so a Hebrew line in a Latin-only font still shows. The font picker marks which fonts cover the text typed.

## Import and export

- Import SRT and WebVTT files onto the subtitle track; times are rounded to the nearest frame and the file's own styling is dropped for the track's.
- Export subtitles as SRT or WebVTT next to the video, as a choice in the export dialog: burned in (drawn into the picture, the default), as a side file, or both.
- MOV and WebM exports (see [Transparent background](transparent-background.md)) can also carry them as a subtitle stream.

## Undo and saving

- Every add, edit, drag and style change is one undo step, named ("Undo Edit text", "Undo Move cue"). Typing in a clip is one step when the edit is finished.
- Text clips and the subtitle track save with the reel in the project file. Styles save in the project.

## Speed

- Text is drawn when the frame is drawn, cached per clip and frame while it does not change, so scrubbing stays smooth with many cues.

## Edge cases

- A shot clip that already has a text layer inside: both show; the reel's text draws above it, because the reel's track order rules.
- Very long lines: wrapped to the title-safe width; a cue over 2 lines shows a small warning on its clip.
- Changing the canvas size keeps text at the same place relative to the frame, not in pixels.

## Hotkeys

Checked against the keys Snapper binds today: T, Shift+T, S, M and D are taken, Ctrl+T and L are free.

| Key | Where | Does |
| --- | --- | --- |
| Ctrl+T | Video tab | Add a text clip at the playhead |
| L | Video tab, playing | Start the next subtitle line at the playhead |
| Double click | Text on the preview or a text clip | Edit the text in place |
| Ctrl+Enter | Editing text | Finish the edit |
| Escape | Editing text | Cancel the edit |
| Alt + drag | Cue edge | Move it together with the next cue's start |

## Open questions

- [ ] Should shot text layers move to the reel, or stay for text that belongs to one shot?
- [ ] Burned in or side file as the default for subtitles?

## Not in the first version

- Karaoke highlight that fills each word as it is sung, and auto-timing subtitles from the audio.
