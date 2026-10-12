# Transparent shot background

Status: planned.

Needs: nothing. It brings the new export formats (PNG sequence, WebM, MOV) that [Sound on the reel](reel-audio.md) and [Text and subtitles](reel-text-and-subtitles.md) also use.

A shot's background is a solid color today (white by default, `model/shot.h`), and export makes MP4 or GIF only, neither of which keeps transparency well. A see-through background lets a shot sit over live video in the reel, or go into another program as a cutout.

## Setting it

- In the shot settings, the Background color swatch gets a Transparent tile beside it. The color stays remembered, so turning Transparent off brings the same color back.
- Also in the [floating palette](palette.md), as the first swatch, so any place that picks a background can pick none.
- New project tiles: a fifth start, Transparent, for projects that will be composited elsewhere.
- Several shots picked on the master track: the setting applies to all of them in one undo step.

## Seeing it

- The stage shows a checkerboard where nothing is drawn. Its two grays are muted so they do not fight the doll's colors; the shade follows the app theme.
- Shot thumbnails on the master track and the reel show the checkerboard too, with a small transparent badge, so a see-through shot is spotted at a glance.
- Hold a key (proposed: Shift+V) to preview over a solid color picked from the palette, to check edges on dark and light backgrounds without changing the setting.
- In the reel, a transparent shot clip shows the tracks below through it, so dolls can play over a video clip directly.

## Export

- New formats that keep transparency: PNG sequence (a folder of numbered frames), WebM (VP9 with alpha, for the web) and MOV (ProRes 4444, for editing programs).
- The export dialog's format row marks which keep transparency. Picking MP4 for a transparent shot shows a note that the background becomes the shot's remembered color.
- GIF: transparent pixels become GIF's one see-through color; soft edges are blended over the remembered color to avoid a dark fringe.
- The song still goes into MOV and WebM; a PNG sequence writes the song next to the frames as a WAV file.

## Effects and edges

- Soft edges (smooth warp, joint circles, line smoother) keep their partial transparency, stored premultiplied so no dark or white halo appears when placed over something else.
- Effects that change everything below (flash, fill, invert, glitch) act only where something is drawn, unless their target is set to include the background.
- Glows and transmitted light reach into the transparent area, so a glow still shows when the shot is placed over video.
- Camera shake and zoom move only what is drawn; the empty area stays empty.

## Undo and saving

- One undo step per change, named ("Undo Transparent background"). Saved in the shot; old projects open with their color as before.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| Shift+V (hold) | Stage, transparent shot | Preview over a solid color while held |

## Not in the first version

- A background picture per shot (the picture layer covers that today).
- Exporting each doll as its own transparent layer.
