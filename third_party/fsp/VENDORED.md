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

## Action lines and bursts (SpeedLines, BurstLines, RadialLines, TouchLines, MuzzleFlash)

### SpeedLines

Copied `SpeedLines/` (`A_Noise.cpp`, `SpeedLines.cpp`,
`SpeedLinesFilter.cpp`, `SpeedLines.h`, `NF_Target.h`, `README.MD`) and
`_NFLib/fx/NF_Blend`, `NF_Blur`, `NF_DrawAALine`, `NF_Mult` (`.cpp`,
`.h`). `A_Noise.cpp` defines `GetMaxChannel` like `NF_Blend.cpp`, so it
has its own unit (`units/speedlines_unit_2.cpp`).

```
SpeedLines/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
SpeedLines/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
SpeedLines/SpeedLines.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
SpeedLines/SpeedLines.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
SpeedLines/SpeedLines.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
SpeedLines/SpeedLines.h:19: "..\_NFLib\fx\NF_ChannelMinmax.h" -> "../_NFLib/fx/NF_ChannelMinMax.h"
SpeedLines/SpeedLines.h:20: "..\_NFLib\fx\NF_ChannelBlur.h" -> "../_NFLib/fx/NF_ChannelBlur.h"
SpeedLines/SpeedLines.h:21: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
SpeedLines/SpeedLines.h:22: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
SpeedLines/SpeedLines.h:23: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
SpeedLines/SpeedLines.h:25: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
SpeedLines/SpeedLines.h:26: "..\_NFLib\fx\NF_Blur.h" -> "../_NFLib/fx/NF_Blur.h"
SpeedLines/SpeedLines.h:27: "..\_NFLib\fx\NF_DrawAALine.h" -> "../_NFLib/fx/NF_DrawAALine.h"
SpeedLines/SpeedLines.h:28: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
_NFLib/fx/NF_Blend.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_DrawAALine.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
```

(Lines 19 to 23 of `SpeedLines.h` sit in a comment.) Checked by eye:
parallel lines on a clear layer, seed moves them, angle, blur, noise
and three colors work. The line count follows the picture's longer
side (the plugin's own rule), so a half scale preview has fewer lines
of half the width.

### BurstLines

Copied `BurstLines/` (`BrustLinesMain.cpp`, `BurstLines.cpp`,
`BurstLines.h`, `NF_Target.h`, `README.MD`), `_NFLib/fx/NF_UnMult` and
`_NFLib/vector/NF_VectorLine`, `NF_VectorMask` (`.cpp`, `.h`; not the
stray `NF_VectorMask.cppqq`). `NF_UnMult.cpp` and `NF_VectorMask.cpp`
repeat helpers of `NF_Mult.cpp` and `NF_Blend.cpp`, so each has its own
unit (`units/burstlines_unit_2.cpp`, `_3.cpp`).

```
BurstLines/BurstLines.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
BurstLines/BurstLines.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
BurstLines/BurstLines.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
BurstLines/BurstLines.h:19: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
BurstLines/BurstLines.h:22: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
BurstLines/BurstLines.h:23: "..\_NFLib\fx\NF_AlphaHyperbolic.h" -> "../_NFLib/fx/NF_AlphaHyperbolic.h"
BurstLines/BurstLines.h:24: "..\_NFLib\fx\NF_Bilateral.h" -> "../_NFLib/fx/NF_Bilateral.h"
BurstLines/BurstLines.h:25: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
BurstLines/BurstLines.h:26: "..\_NFLib\fx\NF_Blur.h" -> "../_NFLib/fx/NF_Blur.h"
BurstLines/BurstLines.h:27: "..\_NFLib\fx\NF_ChannelBlur.h" -> "../_NFLib/fx/NF_ChannelBlur.h"
BurstLines/BurstLines.h:28: "..\_NFLib\fx\NF_ChannelMinmax.h" -> "../_NFLib/fx/NF_ChannelMinMax.h"
BurstLines/BurstLines.h:29: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
BurstLines/BurstLines.h:30: "..\_NFLib\fx\NF_DrawWorld.h" -> "../_NFLib/fx/NF_DrawWorld.h"
BurstLines/BurstLines.h:31: "..\_NFLib\fx\NF_Gradient.h" -> "../_NFLib/fx/NF_Gradient.h"
BurstLines/BurstLines.h:32: "..\_NFLib\fx\NF_Hyperbolic.h" -> "../_NFLib/fx/NF_Hyperbolic.h"
BurstLines/BurstLines.h:33: "..\_NFLib\fx\NF_LogicGray.h" -> "../_NFLib/fx/NF_LogicGray.h"
BurstLines/BurstLines.h:34: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
BurstLines/BurstLines.h:35: "..\_NFLib\fx\NF_Mosaic.h" -> "../_NFLib/fx/NF_Mosaic.h"
BurstLines/BurstLines.h:36: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
BurstLines/BurstLines.h:37: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
BurstLines/BurstLines.h:38: "..\_NFLib\fx\NF_SpatUtils.h" -> "../_NFLib/fx/NF_SpatUtils.h"
BurstLines/BurstLines.h:39: "..\_NFLib\fx\NF_UnMult.h" -> "../_NFLib/fx/NF_UnMult.h"
BurstLines/BurstLines.h:41: "..\_NFLib\vector\NF_VectorLine.h" -> "../_NFLib/vector/NF_VectorLine.h"
BurstLines/BurstLines.h:42: "..\_NFLib\vector\NF_VectorMask.h" -> "../_NFLib/vector/NF_VectorMask.h"
BurstLines/BurstLines.h:44: "..\_NFLib\fx\NF_DrawAALine.h" -> "../_NFLib/fx/NF_DrawAALine.h"
BurstLines/BurstLines.h:45: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
BurstLines/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
BurstLines/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
_NFLib/vector/NF_VectorLine.h:24: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/vector/NF_VectorMask.h:25: "..\NF_Utils.h" -> "../NF_Utils.h"
```

(Lines 22 to 42 of `BurstLines.h` sit in a comment.) Checked by eye:
a burst around the center, seed moves it, direction with angleRange,
sub lines and two colors work, and at half scale distance, length and
thickness halve.

### RadialLines

Copied `RadialLines/` (`A_Noise.cpp`, `RadialLines.cpp`,
`RadialLinesFilter.cpp`, `RadialLines.h`, `RadialLinesFilter.h`,
`NF_Target.h`, `README.MD`) and `_NFLib/fx/NF_AlphaHyperbolic`,
`NF_ChannelBlur`, `NF_ChannelMinMax` (`.cpp`, `.h`). `NF_Blend.cpp`
repeats helpers of `RadialLinesFilter.h` and `NF_ChannelBlur.cpp` those
of `NF_Blur.cpp`, so each has its own unit
(`units/radiallines_unit_2.cpp`, `_3.cpp`).

```
RadialLines/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
RadialLines/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
RadialLines/RadialLinesFilter.h:5: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
RadialLines/RadialLinesFilter.h:6: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
RadialLines/RadialLines.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
RadialLines/RadialLines.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
RadialLines/RadialLines.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
RadialLines/RadialLines.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
RadialLines/RadialLines.h:21: "..\_NFLib\fx\NF_Blur.h" -> "../_NFLib/fx/NF_Blur.h"
RadialLines/RadialLines.h:27: "..\_NFLib\fx\NF_DrawAALine.h" -> "../_NFLib/fx/NF_DrawAALine.h"
RadialLines/RadialLines.h:28: "..\_NFLib\fx\NF_AlphaHyperbolic.h" -> "../_NFLib/fx/NF_AlphaHyperbolic.h"
RadialLines/RadialLines.h:29: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
```

Checked by eye: focus lines from the edges toward the center, seed
moves them, the center point, hyperbolic, blur, noise and two colors
work, and weight and blur halve at half scale.

### TouchLines

Copied `TouchLines/` (`A_Noise.cpp`, `ThresholdLine.cpp`,
`TouchCtLine.cpp`, `TouchLineFilter.cpp`, `TouchLines.cpp`,
`TouchStLine.cpp`, `ThresholdLine.h`, `TouchLineFilter.h`,
`TouchLines.h`, `NF_Target.h`, `README.MD`) and `_NFLib/fx/debug_font`,
`NF_DrawWorld`, `NF_MinMax`, `NF_Noise`, `NF_Paint` (`.cpp`, `.h`),
`debug_font_1424.h` and `NF_Draw.h` (`NF_DrawWorld.cpp` includes it;
`NF_Draw.cpp` is not in TouchLines' project). `A_Noise.cpp`,
`NF_Blend.cpp` and `TouchLineFilter.cpp` repeat helpers of
`ThresholdLine.cpp`, so each has its own unit
(`units/touchlines_unit_2.cpp` to `_4.cpp`).

```
TouchLines/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
TouchLines/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
TouchLines/ThresholdLine.h:25: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
TouchLines/TouchLineFilter.h:5: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
TouchLines/TouchLineFilter.h:6: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
TouchLines/TouchLines.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
TouchLines/TouchLines.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
TouchLines/TouchLines.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
TouchLines/TouchLines.h:19: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
TouchLines/TouchLines.h:20: "..\_NFLib\fx\NF_ChannelMinmax.h" -> "../_NFLib/fx/NF_ChannelMinMax.h"
TouchLines/TouchLines.h:21: "..\_NFLib\fx\NF_ChannelBlur.h" -> "../_NFLib/fx/NF_ChannelBlur.h"
TouchLines/TouchLines.h:22: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
TouchLines/TouchLines.h:23: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
TouchLines/TouchLines.h:24: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
TouchLines/TouchLines.h:25: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
TouchLines/TouchLines.h:29: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
TouchLines/TouchLines.h:30: "..\_NFLib\fx\NF_Blur.h" -> "../_NFLib/fx/NF_Blur.h"
TouchLines/TouchLines.h:31: "..\_NFLib\fx\NF_DrawAALine.h" -> "../_NFLib/fx/NF_DrawAALine.h"
TouchLines/TouchLines.h:32: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
_NFLib/fx/NF_DrawWorld.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_Draw.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_Noise.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_Paint.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
```

(Lines 19 to 27 of `TouchLines.h` sit in a comment.) Checked by eye:
the target mask finds the drawing's edges (luminance and alpha), lines
start on those edges straight along the angle or toward the center,
seed moves them, size halves at half scale. `value` is a chance per
edge pixel (value / 2000), so the default 1 draws almost nothing on a
small picture; that is the plugin's own scale.
