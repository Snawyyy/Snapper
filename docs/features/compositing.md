# Compositing

Status: built, first pass (2026-10-13); owner tests by eye. Inspired by OLM OpenTools and F's Plugins.

The first pass adds a Comp tab after Video and Pose: it works on the Video tab's reel, not on the Pose strip. The playhead is the reel's, the stage shows the reel's picture (every shot with its comp, video clips and text), and the shot on top at the playhead is the one whose stack is edited (`ShotAtReel`, `CompPage::FollowPlayhead`); clicking a clip on the Comp timeline or scrubbing it picks that shot. Every effect on the left under a search field with color marks in two groups, Snapper and F's Plugins (hover shows what one does; double click or Enter adds it; drag it between listed effects, or onto a doll on the stage, which outlines it dashed), the stage in the middle with yellow handles for the picked effect (points A and B, an arrow for a light or a direction; Shift snaps to 15 degrees), the play controls and a Comp timeline (the reel's shot clips, then a row of key marks per effect and one for the shot Light over the picked shot's clip) under it, and on the right the effects on what is picked on the stage (a click picks a doll or piece, outlined in red; a click on nothing picks the screen), in the order they run, each a fold like in After Effects: a square on switch at the far left, a color mark, the name, a slow mark past a frame at 24 fps and the fold arrow at the right; a click on the plate picks it (red edge) and opens or closes it, a drag reorders (Escape cancels), right click copies, pastes below, duplicates, moves or removes, Ctrl+C and Ctrl+V copy and paste across shots (the paste goes on what is picked, keeping its colors), and its controls (Blend, Colors, then its own) inside; the shot Light under them. A new effect goes on what is picked: the screen, the doll, or its picked pieces (the Applies to tiles are gone). Nine kinds: Line smoother, Color swap, Transmitted light (outside or inside), Rim light, Gradient (Linear, Radial, Angle, Diamond, Four-corner, From edge; color stops with their own opacity; Steps), Motion blur, Sparkle, Speed lines (burst or parallel, new every N frames) and Finish (grain, scanlines or posterize), plus a Scene gradient entry. Each narrows to Picked colors from the floating palette or I over the stage; the 19 shared blend modes in a grouped dropdown, Shift+= and Shift+- to step them; keyed knobs with a Key button and a Keys ease switch; a shot Light that rim lights and linear or radial gradients can follow. Each effect is a clip on the Comp timeline (rows in run order): drag it to move it (its keys go along), drag an end to trim, double click to fit it to its shot again, Ctrl drags without snapping to the shot ends and the playhead, Escape cancels. A clip may run past its shot: an effect on the screen goes on over whatever the reel shows there, one on a doll stops at the cut (hatched). Hold V for all effects off, hold K for what the picked effect touches, Delete removes, Ctrl+D duplicates. Effects save in the shot by name (`comp` and `light` in the shot JSON). `FrameRenderer::RenderShot` runs the stack after every layer, so the Pose stage, the Comp stage, thumbnails and export all match. `CompManager` owns every edit and the tab's view state.

Defaults taken for the open points: the comp runs after all layers (today's 7 effect layers stay layers and are not moved into the stack yet); a knob with no key or one is the shot's value, a change keys it at the playhead once it has two keys (Key adds the first two); the shot Light defaults to 315 degrees (upper left) and its keys ease; target colors match within an RGB distance of 24 with a soft edge; an effect on a layer is cut where layers above it cover it, but pieces of the same doll in front of a picked piece are not; Ellipse is left out of the gradient shapes (Radial plus a later stretch knob).

Line smoother, rebuilt (2026-10-13): a rebuild of OLM Smoother v2 from its manual (Apache 2.0 tools by OLM Digital), not its code, replacing the first pass's corner rounding. Controls, as the plugin names them: Strength (Snapper's mix, as on every effect), Smoothness 0 to 100 (0 changes nothing), Extra Smooth 0 to 100, Smooth Range 0 to 255 (colors this close on every channel count as one, so a gradation in a cel does not stop a line; 255 leaves the frame as it is), Use Color Key and Color Key (that color turns clear before smoothing), Smoother Version v1 or v2 (v2 blends in linear light, which thins black lines and mixes colors cleanly; v1 blends sRGB values), Gamma Correction Off, All Colors or Gamma Colors, Gamma Value, Number of Gamma and up to 4 Gamma Colors (black alone keeps black lines as thick as the input while colors still mix in linear light). The panel shows them with the F's Plugins rows (faders, a check box, choices, swatches) and hides the color key, gamma value and gamma colors while their switch is off. The method is MLAA: each run of a stair step whose ends turn into the next step is read as part of a straight line through the middles of the steps, and every pixel takes the share of the color across the edge that the line cuts off; corners, line caps and one-pixel notches at box corners stay sharp. Picked colors keep it to steps touching those colors; a layer keeps it to the layer and the pixel just outside its edge. It works on the frame's own pixels whatever the preview scale. `tools/smoother_render.cpp` (build target `smoother_render`) runs it on a PNG.

Guesses where the manual says nothing: the defaults (Extra Smooth 0, Smooth Range 0, Color Key white, Gamma Value 2.4, Gamma Correction Off); Smooth Range is the largest channel difference in 8-bit values, alpha included; the color key takes only that exact color (one step either way); Extra Smooth is SMAA's smoothing of short patterns (the share grows toward the square root of twice it, fading out over runs 32 pixels long), not OLM's own; in Gamma Colors a pixel mixes with the gamma when it or a neighbour it takes color from is within an RGB distance of 40 of a gamma color; a step's riser may be at most as long as its run (up to 32 pixels) and the next run at least a quarter of it, so steep and shallow parts are each smoothed by one pass. Thin one-pixel lines come out grey in v2, as the manual's "thinner black lines" says; Gamma Colors with black restores them.

Not built yet: blend modes on stage layers (dolls, pictures, text) and the hover preview in the dropdown; line thin and fill, shadow line; Looks; copy and paste of effects between shots (Ctrl+Shift+C and V); the add tiles' moving previews (a menu for now); anime motion blur's direction from the motion; Dither on gradients; double click on a ramp to add a stop (a + button for now); the warning chip for a picked color the doll no longer has; draft size while dragging (the Comp stage keeps drawn frames instead); moving the old effect layers into the stack.

Needs: the [floating palette](palette.md) for color targets.

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
| [OLM Smoother v2](https://www.olm.co.jp/post/olm-smoother-v2) | The Smoother with Smoothness, Extra Smooth, Smooth Range, a color key, sRGB to linear and gamma correction | Its manual describes the controls, not the method; Snapper's Line smoother is rebuilt from it | Same as Smoother |

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
| 1 | Line smoother | OLM Smoother v2 | Smooths pixel-brush stair steps on the final frame without blurring, the anime smooth filter | Strength, Smoothness, Extra Smooth, Smooth Range, color key, version, gamma correction; lines only (pick line colors) or all edges |
| 2 | Color swap | OLM Color Key, ColorSwitch | Swaps picked palette colors for others, keyed, for night scenes, hit flashes, line recolor | Color pairs from the palette; edge thin and feather |
| 3 | Transmitted light | Distance Gradation, Flare, InnerFlare | Glow spreading out from (or into) a shape's edge, for lamps, magic, backlight | Size, falloff curve, color, inside or outside, strength |
| 4 | Rim light | RimFill, Distance Gradation | A thin band of light on the edges of a target that face a light direction, the anime backlit rim | Light direction (dial, keyed), width, color, falloff, strength |
| 5 | Gradient | TargetGrad, TargetGradRadical, Distance Gradation | A gradient laid over the target, often chosen fill colors only: blush on skin, sky fades, a dark corner | Shape (linear, radial, ellipse, four-corner, angle, diamond, from-edge), color stops, handles on the stage |
| 6 | Anime motion blur | OLM Directional Blur | Smears a target along its motion with cartoon streak noise | Direction (auto from motion or set), length, streak noise |
| 7 | Sparkle | OLM KiraKira, Star | Star streaks and glow from the brightest parts | Threshold, streak count, length, rotation |
| 8 | Speed and burst lines | SpeedLines, BurstLines, RadialLines, TouchLines | Drawn lines across the frame or around a point, new each frame or held | Center, density, thickness, color, change every N frames |
| 9 | Line thin and fill | Thin, OLM Toon Dilate | Thins lines, then fills the holes with the nearest colors only | Thin amount; fill on or off |
| 10 | Shadow line | EdgeLine-Hi | Draws a line where two chosen colors meet (shade edge on skin) | Color pair, line color, side, width |
| 11 | Finish | Grain, Scanline, Posterization8bit | Film grain, scanlines, 8-bit posterize over everything | Amount, size, held or moving |

### Rim light

- Lights only the edges of the target that face the light: for each edge pixel, the direction to the outside is compared with the light direction, and a band of the chosen width is lit there, fading inward. The edge distance comes from the same distance pass as transmitted light.
- Light direction: a dial on the effect, keyed like any control, so a light can swing across a shot. A shot Light (one direction shared by every rim light and cel gradient that picks "Shot light") keeps several dolls lit from the same side, set once.
- Follows the doll: the band is computed on the doll's posed shape each frame, so turning, bending and jelly keep the rim on the lit side.
- Inside only: the band never draws past the target's outline, and Picked colors can limit it further (skin and hair, not the outline color).
- Blend: any of the shared blend modes below, Add by default (glow); Screen gives a soft rim, Normal a flat anime rim in the light color.
- On the stage while the effect is picked, a yellow arrow handle sets the direction by dragging, the same feel as other handles.
- F's Plugins' RimFill source is MIT and readable, so its method is checked there before building.

### Gradients

- Shapes, as tiles: Linear (between two points), Radial (a circle from a center), Ellipse (a stretched, turnable circle), Four-corner (bilinear: a color on each corner of a box, blended across it), Angle (sweeps around a center like a clock hand), Diamond, and From edge (by distance from the target's outline, the Distance Gradation way).
- Colors: two by default, more by double clicking the ramp to add a stop; each stop has a color from the [floating palette](palette.md) and its own opacity, so a gradient can fade to clear. Four-corner has four corner colors instead of a ramp.
- Handles on the stage while the effect is picked: yellow points for the ends, center, radius and corners, dragged like every handle; Shift keeps angles to 15 degree steps. Every handle is keyed, so a light can slide across a shot.
- Where it applies: any target, including the whole shot (Everything below) and Picked colors, so a blush lands only on the skin fill. With Picked colors, the gradient keeps the target's outline crisp.
- Follows: on a doll or picked pieces, the gradient is placed in the doll's own space, so it moves and turns with the doll; on the whole shot it stays in screen space.
- Blend: any of the shared blend modes below; Multiply shades, Screen and Add light, Overlay tints.
- Steps: a Steps fader turns the smooth ramp into 2 to 8 flat bands, the hard-edged cel gradient anime uses; 0 keeps it smooth. A Dither option for a pixel-art look.
- Scene gradient: a tile of its own in the Add row that drops a gradient over the whole shot in one click (target Everything below, screen space, Overlay blend), for a sunset tint, a dark vignette or a top-down light. It can also sit at the bottom of the stack as the shot's background, replacing the flat background color with a sky, and keyed handles let that sky change during the shot.
- Shared light: picking "Shot light" aims a linear or radial gradient from the same direction as the rim lights.

### Blend modes

- One shared set, used by every effect and every stage layer (dolls, objects, pictures, text), replacing the short per-effect lists above. One blend function in the renderer, so a mode looks the same everywhere.
- The set, grouped the way artists know it:
    - Normal
    - Darken group: Darken, Multiply, Color Burn, Linear Burn
    - Lighten group: Lighten, Screen, Color Dodge, Add
    - Contrast group: Overlay, Soft Light, Hard Light
    - Compare group: Difference, Exclusion, Subtract
    - Color group: Hue, Saturation, Color, Luminosity
- Most map onto Qt's own painter composition modes; Linear Burn, Subtract and the Color group are written once in the renderer.
- Picking: a sunken dropdown in the effect or layer box, grouped as above. Hovering a mode in the open list previews it live on the stage, so modes can be compared without committing; the pick is one undo step.
- Cycling: Shift+= and Shift+- step the picked layer's or effect's mode up and down the list, the same as in painting programs, with a chip naming the mode.
- Strength: the existing opacity fader blends the result with what is below, separate from the mode.
- Keyed as a step: the mode can change at a key (a flash that switches to Add for two frames) but never blends between two modes.
- Transparent shots: modes act on what is drawn below; over the empty, see-through area they behave as Normal, so exports with alpha stay clean ([Transparent background](transparent-background.md)).
- Saved by name in the project, so a renamed or reordered list never changes a finished shot.

### Targets

- Every effect has an Applies to row of tiles: Everything below (today's behavior), A layer (a doll or picture), Picked pieces, Picked colors.
- Picked colors use the [floating palette](palette.md): click swatches (Shift adds, Ctrl flips) or press I and click the stage to pick a color under the cursor.
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
| Shift+= and Shift+- | Layer or effect picked | Next and previous blend mode |

### Not in the first version

- A node graph like Nuke or OpenToonz. The stack keeps it simple and game like; nodes only if the stack proves too small.
- Particles (snow, rain) and 3D light.

## Sources

- [OLM OpenTools list](https://www.olm.co.jp/rd/categories/opentools?lang=en), with the tool pages for [Smoother](https://www.olm.co.jp/post/olm-smoother), [Toon Dilate](https://www.olm.co.jp/post/olm-toon-dilate), [Color Key](https://www.olm.co.jp/post/olm-color-key), [Distance Gradation](https://www.olm.co.jp/post/distance-gradation), [KiraKira](https://www.olm.co.jp/post/olm-kirakira) and [Directional Blur](https://www.olm.co.jp/post/olm-directional-blur)
- [F's Plugins on GitHub](https://github.com/bryful/F-s-PluginsProjects)
- Snapper's own code for today's effect layers
