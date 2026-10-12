# Compositing

Status: planned. Inspired by OLM OpenTools and F's Plugins.

A Compositing tab for Snapper, with effects reverse engineered from the tools Japanese anime studios use.

## What Snapper has today

Snapper has 7 effect layers: flash, fill, invert, zoom punch, halftone, glitch and posterize. Each changes everything drawn below it, like an adjustment layer, with one color and one keyed amount from 0 to 1.

- They are drawn in screen space, after the camera, so they do not move with a doll.
- They cannot target one doll, one piece or one color. Anime compositing needs exactly that: most of its effects work on a chosen color or a chosen cel, not the whole frame.

## OLM open tools

The Japanese animation studio OLM gives away 8 compositing tools under the Apache 2.0 license. Most are After Effects plugins for Windows; the pages describe what each does but rarely the method.

| Tool | What it does | How it works, as far as the page says | Use in anime |
| --- | --- | --- | --- |
| [OLM Smoother](https://www.olm.co.jp/post/olm-smoother) | Smooths stair-stepped lines without blurring them | Like Morphological Antialiasing (MLAA): finds edge patterns and smooths along them | Clean up scanned black-and-white line art after coloring |
| [OLM Toon Dilate](https://www.olm.co.jp/post/olm-toon-dilate) | Paints transparent pixels with the closest opaque pixel's color | No blending, so only colors already in the art appear | Fill holes left after thinning or moving outlines; change line style keeping the palette |
| [OLM Color Key](https://www.olm.co.jp/post/olm-color-key) | Keys several colors at once and can replace them | Color key plus OLM's Color Keep, choice of color model, edge thin and feather | Pick cel colors to isolate or recolor them |
| [Distance Gradation](https://www.olm.co.jp/post/distance-gradation) | A gradient by distance from the alpha edge | Distance transform mapped to a gradient | Transmitted light and aura glow around a shape |
| [OLM KiraKira](https://www.olm.co.jp/post/olm-kirakira) | Glows and star streaks from the bright parts of a picture | Highlights extended into glows; streak length, rotation, quality | Sparkles on metal, eyes, water |
| [OLM Directional Blur](https://www.olm.co.jp/post/olm-directional-blur) | Smears the picture in one direction, keeping color strength | Anisotropic blur; with noise it gives cartoon motion lines | Anime-style motion blur |
| [OLM Blur](https://www.olm.co.jp/rd/categories/opentools?lang=en) | A blur made for cel animation | Not described | Soft focus on cels |
| [OLM Smoother v2](https://www.olm.co.jp/post/olm-smoother-v2) | A stronger version of the Smoother | Not described | Same as Smoother |

## F's Plugins

[F's Plugins](https://github.com/bryful/F-s-PluginsProjects) by bryful is about 50 After Effects plugins for Windows under the MIT license, grouped as Cell (for unsmoothed anime cels), Channel, Colorize, Draw, Filter and Noise. The author's newer line is NF-Plugins; F's Plugins get no new plugins since February 2026. The README describes only a dozen of them.

| Plugin | What it does |
| --- | --- |
| EdgeLine-Hi | Draws a line on the border between two chosen colors, in a chosen direction |
| Flare | Glow from a black-and-white mask, as transmitted light |
| PixelExtend | Grows a chosen color in a chosen direction |
| graytoneToColorize | Simple gradient map from gray to colors, for gold and heat-camera looks |
| Posterization8bit | Posterize computed in 8 bits, unlike the stock one |
| grayToContourLine | Posterize with uneven high, middle and low steps |
| grayToWaveLine | Draws a gray picture as a plotter-style wave |
| Scanline | Black lines on even rows, like an old monitor |
| YuvControl | Brightness and color controls in YUV |
| TargetGrad, TargetGradRadical | Gradients (beta) |

Listed without a description, but named for anime compositing work: AlphaFill, BurstLines, CellLineEraser, ColorSwitch, FloodFill, Grain, InkingLineBlur, InnerFlare, LineExtraction, MainLineReplace, MuzzleFlash, Outline, RadialLines, RimFill, SelectiveColorBlur, SpeedLines, Sputtering, Star, Thin, TouchLines, WhiteKeying. Their behavior below is inferred from the name and must be checked in the code before building.

## The Compositing tab

A Comp tab beside Pose, Rig and Video, where a shot's finished frame gets the anime finishing pass: smoothed lines, recolored lines, glows, gradients and speed effects. The core change is targets: an effect can work on one doll, some pieces, or a few palette colors, not only on everything below it. F's Plugins source is MIT licensed and readable, so its effects can be studied line by line; OLM ships binaries, so those are rebuilt from what they do.

### Effects to build, ranked

| Rank | Effect | From | What it does in Snapper | Main controls |
| --- | --- | --- | --- | --- |
| 1 | Line smoother | OLM Smoother | Smooths pixel-brush stair steps on the final frame without blurring, the anime smooth filter | Strength; lines only (pick line colors) or all edges |
| 2 | Color swap | OLM Color Key, ColorSwitch | Swaps picked palette colors for others, keyed, for night scenes, hit flashes, line recolor | Color pairs from the palette; edge thin and feather |
| 3 | Transmitted light | Distance Gradation, Flare, InnerFlare | Glow spreading out from (or into) a shape's edge, for lamps, magic, backlight | Size, falloff curve, color, inside or outside, strength |
| 4 | Cel gradient | TargetGrad, Distance Gradation | A gradient laid over chosen fill colors only: blush on skin, sky fades | Two colors, direction or from-edge, picked fills |
| 5 | Anime motion blur | OLM Directional Blur | Smears a target along its motion with cartoon streak noise | Direction (auto from motion or set), length, streak noise |
| 6 | Sparkle | OLM KiraKira, Star | Star streaks and glow from the brightest parts | Threshold, streak count, length, rotation |
| 7 | Speed and burst lines | SpeedLines, BurstLines, RadialLines, TouchLines | Drawn lines across the frame or around a point, new each frame or held | Center, density, thickness, color, change every N frames |
| 8 | Line thin and fill | Thin, OLM Toon Dilate | Thins lines, then fills the holes with the nearest colors only | Thin amount; fill on or off |
| 9 | Shadow line | EdgeLine-Hi | Draws a line where two chosen colors meet (shade edge on skin) | Color pair, line color, side, width |
| 10 | Finish | Grain, Scanline, Posterization8bit | Film grain, scanlines, 8-bit posterize over everything | Amount, size, held or moving |

### Targets

- Every effect has an Applies to row of tiles: Everything below (today's behavior), A layer (a doll or picture), Picked pieces, Picked colors.
- Picked colors use the floating palette from the drawing feature: click swatches (Shift adds, Ctrl flips) or press I and click the stage to pick a color under the cursor.
- Targets combine: "skin color on the doll Mika only".
- An effect on a doll follows the doll; one on Everything stays in screen space after the camera, as today.
- Show mask (hold K): the stage shows what the effect touches in white over black, to check a color pick.

### Working in the tab

- Left: the shot's effect stack as rows (eye, kind icon, name, target chip), drag to reorder, the top one drawn last. Center: the stage. Right: the picked effect's controls as faders and dials, same feel as the Pose box. Bottom: the timeline with an amount row per effect.
- Add: a "+" tile row of effect kinds with a moving preview on each tile, made from the current frame. Proposed key: A opens it.
- Every control that makes sense is keyed, not only the amount, and keys hold unless set to ease, as everywhere.
- Hold V to see the frame with all effects off, let go to see them back. The eye on a row turns one effect off.
- Looks: save the stack as a Look ("Night", "Flashback") and apply it to other shots; a Look can stay linked so changing it changes every shot using it.
- Copy and paste one effect or the whole stack between shots (Ctrl+Shift+C and V, the existing settings copy). Ctrl+D duplicates an effect, Delete removes it.
- Today's 7 effect layers move into this tab as effect kinds, so old projects open with the same stack.

### Speed

- Effects render after the frame, cached by frame and settings, so scrubbing back is instant.
- While dragging a control or scrubbing, the preview may run at draft size; it redraws full when you stop. Export always full.
- Heavy effects (glow, blur) show a small clock on their row when they cost more than a frame at 24 fps, so the slow one is easy to find.

### Undo and saving

- Every drag is one undo step, named ("Undo Glow size"); Escape cancels a drag. Undo stays per shot, as now.
- Effects save in the shot, Looks in the project. Effect colors are stored as colors, not palette slots, so a doll edit never changes a finished shot.

### Edge cases

- A picked color that a doll no longer has (after a Krita re-export): the target chip shows a warning and the effect touches nothing until re-picked.
- Smooth warp on and Line smoother on: both run; the smoother then only cleans what the warp left.
- Effects on a layer that is off screen cost nothing.

### Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| A | Comp tab | Add an effect |
| V (hold) | Comp tab | All effects off while held |
| K (hold) | Comp tab | Show what the picked effect touches |

### Not in the first version

- A node graph like Nuke or OpenToonz. The stack keeps it simple and game like; nodes only if the stack proves too small.
- Particles (snow, rain) and 3D light.

## Sources

- [OLM OpenTools list](https://www.olm.co.jp/rd/categories/opentools?lang=en), with the tool pages for [Smoother](https://www.olm.co.jp/post/olm-smoother), [Toon Dilate](https://www.olm.co.jp/post/olm-toon-dilate), [Color Key](https://www.olm.co.jp/post/olm-color-key), [Distance Gradation](https://www.olm.co.jp/post/distance-gradation), [KiraKira](https://www.olm.co.jp/post/olm-kirakira) and [Directional Blur](https://www.olm.co.jp/post/olm-directional-blur)
- [F's Plugins on GitHub](https://github.com/bryful/F-s-PluginsProjects)
- Snapper's own code for today's effect layers
