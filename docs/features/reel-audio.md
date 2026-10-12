# Sound on the reel

Status: built, first pass (2026-10-12); owner tests by eye.

First pass: sound tracks under the picture tracks (2 by default, up to 8; Add sound track in a track's menu) with a header showing the name, Mute (M), Solo (S) and a volume fader (drag; double click back to 0 dB). Sound files (WAV, MP3, OGG, FLAC, M4A, AAC, Opus) drop on a sound track or anywhere on the reel (first sound track with room at the drop point, several in a row, one undo step), or come in through Import... on the Video tab. Sound clips move, trim, split, copy, paste, ripple delete and snap like video clips; they never land on a picture track or the other way round. Each clip draws its waveform (worked out once when the file loads), dimmed on a track that is not heard, and a yellow volume line: drag it for the whole clip (value chip in dB, double click resets); a picked clip shows fade handles on its top corners to drag inward. The song becomes a clip from frame 0 on the first sound track when an old project opens or a song is chosen. Playback and export mix the heard tracks with clip volumes, fades and track volumes; the Export dialog has Sound only (WAV). On the Pose tab a shot placed on the reel plays the reel's sound under its clip (the clip last picked on the Video tab, else the first); the wave band's name says "From clip at 0:42" and a click on it lists the shot's clips to pick another; frames the clip leaves out are silent and hatched in the wave band. A shot not on the reel plays the song as before. A sound file that can't be read keeps its clip, red, named "(find file)".

Not built yet: volume points (Ctrl click) and keyed volume, crossfading overlapped fades, linked sound from video files (and Unlink), scrub sound and its setting, the level meter, the Sounds row in the Cast panel, renaming a track, a file finder for a missing file, and "last played" for picking a shot's clip (only last picked). Sound past the last picture clip plays on the reel but is not exported, so an old project's export keeps its length. Mixing happens on play, so an edit made while playing is heard on the next play.

Needs: nothing for the tracks; the export formats from [Transparent background](transparent-background.md) for MOV, WebM and PNG sequence sound.

A project has one song today, played under the shots and the reel. There is no way to add a sound effect, a voice take or a second music cue. This adds audio tracks to the reel, where all sound lives, and lets the Pose tab play whatever sound the reel has under the shot being posed.

## The rule for shots

- Shots have no sound of their own. Sound lives only on the reel's audio tracks.
- When a shot is placed on the reel, the Pose tab plays the reel's sound under that clip: a shot laid over the chorus plays the chorus while it is posed, and the playhead's frame in the shot matches the clip's frame in the song.
- A shot used in more than one reel clip: the Pose tab plays the sound under the clip last picked or played on the Video tab, and a small chip above the Pose timeline names it ("Sound from clip at 0:42") with a dropdown to pick another clip.
- A shot not on the reel yet: plays the song from where the shot sits on the master track, as today, so nothing changes for projects without a reel.
- Clips that only play part of a shot: the frames of the shot outside the clip are silent in the Pose tab, with the timeline shaded there so it is clear why.

## Audio tracks

- Audio tracks sit under the video tracks on the reel, 2 by default, up to 8. Each has a header with a name, Mute and Solo buttons, and a volume fader.
- The song becomes the first clip on the first audio track when an old project opens, so its timing does not change.
- Add sound: drop WAV, MP3, OGG or FLAC files on an audio track, or on the reel anywhere (it lands on the first free audio track at the drop point). Dropping several lays them in a row.
- Audio clips move, trim, split (S), copy, paste, ripple delete and snap exactly like video clips, through `ReelManager`, and snap to cut markers too.
- Video files with sound: their sound comes in as a linked audio clip under the video clip. Linked clips move and trim together; right click Unlink to separate them.

## Volume and fades

- Each clip has a volume line drawn across its waveform. Drag it up or down for the whole clip; Ctrl click on it adds a point, so volume can be keyed over time (duck the song under a voice line). Points snap to frames.
- Fade handles on the top corners of a clip: drag inward to fade in or out. Two touching clips on one track can crossfade by overlapping their fades, like video transitions.
- The volume line, fades and track volumes are what the export mixes.
- Volumes are shown in decibels in the value chip, from silent to +6 dB.

## Seeing and hearing it

- Each audio clip shows its waveform, drawn once when added and cached, so zooming the reel stays fast.
- Scrubbing plays a short slice of sound at the playhead, on the Video and Pose tabs, so a hit can be found by ear. A setting turns scrub sound off.
- A level meter beside the transport shows the mix while playing, turning red past 0 dB.
- Cut markers can be dropped while any audio plays (M, as today), not only the song.

## A sound library

- A Sounds row in the Cast panel, like dolls and objects: sound files dropped there stay available to every project, with a play button on each card for a quick listen.
- Dragging a sound card onto the reel adds the clip at the drop point.

## Export

- The export mixes every unmuted audio track into the video's sound (MP4, MOV, WebM).
- Export sound only: a WAV of the mix, for finishing the sound in another program.
- PNG sequence exports write the mix next to the frames ([Transparent background](transparent-background.md)).

## Undo and saving

- Every add, move, trim, volume point, fade and track change is one undo step, named ("Undo Fade out", "Undo Move sound").
- Audio clips save with the reel in the project file, by path, like video clips; a missing file keeps its clip with a "find file" chip.

## Edge cases

- Sound under a hidden or muted track still shows its waveform, dimmed.
- Audio at a sample rate other than the export's is resampled once on export.
- A clip trimmed past its file's end shows the tail hatched, as video clips do.

## Hotkeys

No new keys: S, M, Delete, Shift+Delete and the copy keys work on audio clips as on video clips. Mute and Solo are buttons on each track header.

## Open questions

- [ ] When a shot is on the reel several times, should the Pose tab follow the last picked clip (planned) or always the first one? First pass: the last picked, else the first; not remembered across sessions.
- [ ] Should sound effects be allowed inside a shot for timing, if they are always moved to the reel when the shot is placed? First pass: no, sound lives only on the reel.

## Not in the first version

- Audio effects (echo, EQ), recording a voice inside Snapper, and lip sync from audio.
