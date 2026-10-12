# Floating palette

Status: planned. Shared by [Draw on a piece](draw-on-a-piece.md), [Joint circles](joint-snap.md), [Skins](skins.md), [Compositing](compositing.md) (color swap, gradients, rim light, targets), [Text and subtitles](reel-text-and-subtitles.md) and [Transparent background](transparent-background.md).

Needs: nothing; build it early, six features use it.

Dolls are drawn with a few flat colors, so the colors that matter are the ones already in the art. The floating palette is one small panel, used everywhere Snapper asks for a color, that offers those colors first. Building it once keeps every feature picking colors the same way.

## What it shows

- Piece tab: every color in the picked piece's drawings, biggest areas first. Only fully solid pixels count, so soft edges do not flood it with near-duplicates.
- Doll tab: every color of the whole doll, for a color the piece never used.
- Skin colors: while a skin is on, its drawings' colors come first ([Skins](skins.md)).
- Recent: the last 8 colors picked anywhere in the project.
- Added: colors added by hand, kept with the doll.
- A large swatch of the current color, with its hex value.

## Using it

- It floats over the stage, opens where it was last left, can be moved anywhere and folds down to the current color only. One palette at a time; opening it for another feature moves it there.
- Click a swatch to pick. In places that take several colors (effect targets), Shift click adds and Ctrl click flips, the same as picking anywhere in Snapper.
- Hover a swatch: the pixels of that color flash on the stage, so you see where it is used before picking it.
- Pick from the stage: I (or Alt held in draw mode) picks the color under the cursor; the palette jumps to that swatch.
- Right click: Add color (a color wheel and a hex field), Remove added color, Copy hex.
- Transparent appears as the first swatch only where it means something (a shot background, a gradient stop).

## Rules

- The palette reads colors; it never changes a drawing. Swapping colors in the art is the Comp tab's color swap.
- Colors are stored as values wherever they are used, never as palette slots, so editing the doll later never changes a finished shot by surprise.
- Reading a drawing's colors happens once per drawing and is cached with the smooth-warp cache, so opening the palette is instant.

## Undo and saving

- Picking a color is part of the edit that uses it (one undo step, named by that edit). Adding or removing a hand-added color is one step.
- Added colors save in the doll's rig file; recent colors save in the project.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| I | Stage, palette open | Pick the color under the cursor |
| Alt (hold) | Draw mode | Pick the color under the cursor while held |

## Not in the first version

- Named palettes shared across dolls, and importing palette files (.gpl from Krita).
