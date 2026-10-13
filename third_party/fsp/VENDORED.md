# F's Plugins, vendored

- Source: https://github.com/bryful/F-s-PluginsProjects (bryful), MIT
  licence, kept as `LICENSE` here. Credit it in Snapper's About/Help.
- Commit: db6dad39 (2026-05-04).
- Design: `docs/features/fsp-effects.md`.

## What is here

| Path | What |
|---|---|
| `_NFLib/` | F's shared code, only the files the carried plugins use: `AE_SDK.h`, `NF_AE.h`, `NF_Utils.h`, `NF_ParamsSetup.h`, `NF_CATEGORY.h`, `NF_Version.h` |
| `RimFill/` | RimFill's own sources (`RimFill.cpp`, `RimFill.h`, `RimFillSub.cpp`, `RimFillFilter.cpp`, `RimFillFilter.h`, `NF_Target.h`, `README.MD`) |
| `shim/` | Snapper's stand-in for the After Effects host (our code, not F's) |
| `units/` | One wrapper per plugin that compiles its sources in a namespace of its own |
| `registry.cpp` | The list of carried plugins, one line each |
| `include/fsp_host.h` | The plain C interface the rest of Snapper uses |
| `patch_includes.py` | Rewrites Windows include lines (below) |

Files are copied byte for byte (UTF-8 with BOM, some Shift-JIS comments,
CRLF in places) apart from the patches listed below. Never rewrite an
algorithm; if a plugin needs more than an include fix, change the shim.

Not compiled, though F's projects list them: `NF_Json.cpp`,
`NF_Settings.cpp`, `tinyfiledialogs.c` (preset files through Windows
dialogs; `shim/src/nf_settings.cpp` stands in and fails loudly) and the
Adobe `Util/*.cpp` helpers (the shim replaces them).
`RimFillFilter.cpp` is compiled although nothing in RimFill calls it.

## Patches

Only include lines, made by
`python3 patch_includes.py <F's src folder> <files>`: backslashes become
slashes and each name gets the case of the real file.

```
RimFill/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
RimFill/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
RimFill/RimFillFilter.h:5: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
RimFill/RimFillFilter.h:6: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
RimFill/RimFill.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
RimFill/RimFill.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
RimFill/RimFill.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
RimFill/RimFill.h:19: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
RimFill/RimFill.h:20: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
RimFill/RimFill.h:21: "..\_NFLib\fx\NF_blur.h" -> "../_NFLib/fx/NF_Blur.h"
RimFill/RimFill.h:22: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
RimFill/RimFill.h:23: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
RimFill/RimFill.h:24: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
RimFill/RimFill.h:25: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
```

(Lines 19 to 25 of `RimFill.h` sit in a comment; patched anyway.)

## Adding a plugin

1. Copy the plugin's folder (its `*.cpp`, `*.h`, `README.MD`; not `Win/`
   or `*.r`) and whatever of `_NFLib` it uses that is not here yet
   (`_NFLib/fx/*` and `_NFLib/vector/*` files named in its
   `Win/<Plugin>.vcxproj`; copy `json.hpp`, `NF_Settings.h` and
   `tinyfiledialogs.h` together if it includes `NF_Settings.h`).
2. Run `python3 patch_includes.py <F's src folder>` on the copied files
   and paste its output under Patches.
3. Write `units/<prefix>_unit.cpp` like `units/rimfill_unit.cpp`: the
   prelude, the two renames, then every `ClCompile` source of the
   vcxproj (minus the ones listed as not compiled above) inside
   `namespace fsp_<prefix> { ... }`. If two of its sources clash in one
   unit (same file-local helper names: SpeedLines, InnerFlare, AlphaFill,
   SelectiveColorBlur, Grain, Star, Sputtering, InkingLineBlur do), put
   each clashing source in its own `units/<prefix>_unit_<n>.cpp` with
   the same prelude, renames and namespace.
4. Add `PLUGIN(<Folder>, <prefix>)` to `FSP_PLUGINS` in `registry.cpp`.
5. Build `fsp_render`, run it with no arguments to see the parameters,
   then on a picture, and look at the result.

A check on 2026-10-13 compiled all 26 tier 1 plugins of the design
(Outline, RimFill, Thin, ColorSwitch, SpeedLines, TouchLines, InnerFlare,
InnerFlareRot, LineExtraction, WhiteKeying, AlphaFill, FloodFill,
SelectiveColorBlur, Grain, BurstLines, RadialLines, Star, CellLineEraser,
MuzzleFlash, Sputtering, InkingLineBlur, TargetGrad, TargetGradRadical,
Posterization, MaxBlur, MinMax) this way, one unit per source, linked
them, ran every PARAMS_SETUP and rendered each with its defaults without
an error. None of their pictures were looked at yet.

## What the stand-in host does

- Sends GLOBAL_SETUP and PARAMS_SETUP once per plugin, then RENDER per
  call, 8 bit ARGB, straight alpha, one frame at time 0 (24 fps).
- Records parameters from `add_param`; serves them back in RENDER and
  through `checkout_param`. Point values arrive in pixels of the current
  picture, scaled from the percent Snapper stores; angle and fixed
  values in 16.16. A second layer parameter gets an empty layer.
- Downsampling: `downsample_x/y` is the preview scale as a small
  fraction; `ae->downScale` then shrinks widths as in After Effects.
- Iterate suites (8, 16, float; per pixel, with origin, generic) run on
  up to 16 threads. Worlds (new, dispose, pixel format), copy and
  copy_hq (nearest stretch), fill, blend, premultiply and
  premultiply_color, handles, the C library calls, `sprintf_s` and
  `OutputDebugStringA` work.
- Fails loudly (prints `fsp: ... is not supported` and the render gives
  back an error) for: convolve, gaussian_kernel, transform_world,
  transfer_rect, composite_rect, sub- and area sampling, iterate_lut,
  masks and paths, custom UI, AEGP scripts, streams and effects, colored
  16 bit and float fills, copies between depths, NF_Settings presets.
- Never sends: smart render, frame setup, user changed param, update
  params UI, sequence data, other layers or other times. Plugins that
  need those (CameraShake and the like) wait until it does.

## Line work group (Outline, Thin, LineExtraction, CellLineEraser, InkingLineBlur)

### Outline

Sources `Outline/` (`Outline.cpp`, `Outline.h`, `OutlineFilter.cpp`,
`OutlineFilter.h`, `NF_Target.h`, `README.MD`) and from `_NFLib/fx/`:
`debug_font`, `debug_font_1424.h`, `NF_AlphaHyperbolic`, `NF_Blend`,
`NF_Blur`, `NF_ChannelBlur`, `NF_ChannelMinMax`, `NF_Draw`,
`NF_DrawWorld`, `NF_MinMax`, `NF_Mult`, `NF_Noise`, `NF_Paint` (`.h`
and `.cpp`). Two units: `NF_Blur`/`NF_ChannelBlur` and
`NF_MinMax`/`NF_ChannelMinMax` share file-local names.

```
Outline/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
Outline/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
Outline/OutlineFilter.h:5: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
Outline/OutlineFilter.h:6: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
Outline/Outline.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
Outline/Outline.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
Outline/Outline.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
Outline/Outline.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
Outline/Outline.h:19: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
Outline/Outline.h:20: "..\_NFLib\fx\NF_ChannelMinmax.h" -> "../_NFLib/fx/NF_ChannelMinMax.h"
Outline/Outline.h:21: "..\_NFLib\fx\NF_Blur.h" -> "../_NFLib/fx/NF_Blur.h"
Outline/Outline.h:22: "..\_NFLib\fx\NF_ChannelBlur.h" -> "../_NFLib/fx/NF_ChannelBlur.h"
Outline/Outline.h:23: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
Outline/Outline.h:24: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
Outline/Outline.h:25: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
Outline/Outline.h:26: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
Outline/Outline.h:27: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
_NFLib/fx/NF_Blend.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_Draw.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_DrawWorld.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_Noise.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_Paint.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
```

Checked by eye: a disc and a box on transparent; Outer 4 and Inner 2
give a 6 pixel ring (3 at scale 0.5), the fill dropped unless Blend
with original is on.

### Thin

Sources `Thin/` (`Thin.cpp`, `Thin.h`, `Thinsub.cpp`, `NF_Target.h`,
`README.MD`), no `_NFLib/fx` files. One unit.

```
Thin/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
Thin/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
Thin/Thin.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
Thin/Thin.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
Thin/Thin.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
```

Checked by eye: black lines 9 and 10 pixels wide between red and blue
cels; ThinValue 2 takes 2 pixels off each side (1 at scale 0.5) and
fills them with the cel next to them.

### LineExtraction

Sources `LineExtraction/` (`LineExtraction.cpp`, `LineExtraction.h`,
`LineExtractionFilter.cpp`, `LineExtractionFilter.h`, `Level.cpp`,
`Bilateral.cpp`, `NF_Target.h`, `README.MD`; not the `NF_Bilateral.zip`
archive) and `_NFLib/fx/NF_Blend` and `NF_ChannelMinMax` (already
here). Three units: `PixelTraits` is in both `NF_Blend.cpp` and
`LineExtractionFilter.h`, `GetMaxChannel` in both `Level.cpp` and
`LineExtractionFilter.cpp` (same bodies).

```
LineExtraction/LineExtractionFilter.h:5: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
LineExtraction/LineExtractionFilter.h:6: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
LineExtraction/LineExtraction.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
LineExtraction/LineExtraction.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
LineExtraction/LineExtraction.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
LineExtraction/LineExtraction.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
LineExtraction/LineExtraction.h:19: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
LineExtraction/LineExtraction.h:20: "..\_NFLib\fx\NF_ChannelMinmax.h" -> "../_NFLib/fx/NF_ChannelMinMax.h"
LineExtraction/LineExtraction.h:21: "..\_NFLib\fx\NF_Blur.h" -> "../_NFLib/fx/NF_Blur.h"
LineExtraction/LineExtraction.h:22: "..\_NFLib\fx\NF_ChannelBlur.h" -> "../_NFLib/fx/NF_ChannelBlur.h"
LineExtraction/LineExtraction.h:23: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
LineExtraction/LineExtraction.h:24: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
LineExtraction/LineExtraction.h:25: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
LineExtraction/LineExtraction.h:26: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
LineExtraction/LineExtraction.h:27: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
LineExtraction/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
LineExtraction/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
```

Checked by eye: flat color shapes on a tan ground; lines come out at
each lightness edge in the chosen color, stronger where the lightness
jump is bigger and none between two colors of equal lightness (as its
`(max + min) / 2` lightness says); Outer 4 and Inner 4 give 8 pixel
lines, 4 at scale 0.5; Blend with original lays them over the picture.

### CellLineEraser

Sources `CellLineEraser/` (`CellLineEraser.cpp`, `CellLineEraser.h`,
`CellLineEraserSub.cpp`, `NF_Target.h`, `README.MD`), no `_NFLib/fx`
files. One unit.

```
CellLineEraser/CellLineEraser.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
CellLineEraser/CellLineEraser.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
CellLineEraser/CellLineEraser.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
CellLineEraser/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
CellLineEraser/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
```

Checked by eye: red and blue cels on white with 3 pixel black lines
and a red stroke; the target colors vanish and the cels grow into
them, Make White Transparent clears the white. It has no pixel width
parameter, so the preview scale does not change it.
