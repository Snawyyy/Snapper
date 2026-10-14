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
## Color and keying group (ColorSwitch, WhiteKeying, AlphaFill, FloodFill, SelectiveColorBlur)

### ColorSwitch

Copied `ColorSwitch/` (`ColorSwitch.cpp`, `ColoeSwitchSub.cpp`,
`ColorSwitchParams.cpp`, `ColorSwitch.h`, `NF_Target.h`, `README.MD`)
and, for its preset buttons, `_NFLib/NF_Settings.h`, `_NFLib/json.hpp`,
`_NFLib/tinyfiledialogs.h`. Unit: `units/colorswitch_unit.cpp`.

```
ColorSwitch/ColorSwitch.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
ColorSwitch/ColorSwitch.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
ColorSwitch/ColorSwitch.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
ColorSwitch/ColorSwitch.h:17: "..\_NFLib\\NF_Settings.h" -> "../_NFLib//NF_Settings.h"
ColorSwitch/ColorSwitch.h:19: "..\_NFLib\json.hpp" -> "../_NFLib/json.hpp"
ColorSwitch/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
ColorSwitch/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
```

Colors 4 to 31 are set up hidden (`PF_PUI_INVISIBLE`); After Effects
shows them through user changed param when ActiveParamCount grows, which
this host never sends, so they keep a value slot but stay hidden. The
three buttons (All turn off, Save, Load) only act in user changed param.

### WhiteKeying

Copied `WhiteKeying/` (`WhiteKeying.cpp`, `WhiteKeying.h`,
`WhiteKeyingFilter.cpp`, `WhiteKeyingFilter.h`, `NF_Target.h`,
`README.MD`). Unit: `units/whitekeying_unit.cpp`.

```
WhiteKeying/WhiteKeying.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
WhiteKeying/WhiteKeying.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
WhiteKeying/WhiteKeying.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
WhiteKeying/WhiteKeyingFilter.h:5: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
WhiteKeying/WhiteKeyingFilter.h:6: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
WhiteKeying/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
WhiteKeying/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
```

### AlphaFill

Copied `AlphaFill/` (`AlphaFill.cpp`, `AlphaFillFilter.cpp`,
`ChromaKey.cpp`, `AlphaFill.h`, `NF_Target.h`, `README.MD`). Units:
`units/alphafill_unit_1.cpp` to `_3.cpp`, one per source (the filter
and the key both define `ChannelTypeHelper` and `GetMaxChannel`).

```
AlphaFill/AlphaFill.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
AlphaFill/AlphaFill.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
AlphaFill/AlphaFill.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
AlphaFill/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
AlphaFill/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
```

Thresh, Soft and Spill amt are set up hidden; After Effects shows them
through user changed param when Mode is ColorKey, which this host never
sends, so they keep their slots but stay hidden. F's GetParams reads
Spill amt from the Soft slot; kept as written.

### FloodFill

Copied `FloodFill/` (`FloodFill.cpp`, `FloodFill.h`, `NF_Target.h`,
`README.MD`) and the `_NFLib/fx` files its vcxproj names: `debug_font`,
`NF_AlphaHyperbolic`, `NF_Blur`, `NF_Draw`, `NF_DrawWorld`, `NF_MinMax`,
`NF_Mult`, `NF_Noise`, `NF_Paint` (`.cpp` and `.h` each) and
`debug_font_1424.h`. Unit: `units/floodfill_unit.cpp`, every source in
one (no clash).

```
FloodFill/FloodFill.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
FloodFill/FloodFill.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
FloodFill/FloodFill.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
FloodFill/FloodFill.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
FloodFill/FloodFill.h:19: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
FloodFill/FloodFill.h:20: "..\_NFLib\fx\NF_blur.h" -> "../_NFLib/fx/NF_Blur.h"
FloodFill/FloodFill.h:21: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
FloodFill/FloodFill.h:22: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
FloodFill/FloodFill.h:23: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
FloodFill/FloodFill.h:24: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
FloodFill/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
FloodFill/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
_NFLib/fx/NF_Draw.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
_NFLib/fx/NF_DrawWorld.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
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

### InkingLineBlur

Sources `InkingLineBlur/` (`InkingLineBlur.cpp`, `InkingLineBlur.h`,
`InkingLineBlurFilter.cpp`, `InkingLineBlurFilter.h`,
`NF_SelectPixels.cpp`, `NF_SelectPixels.h`, `NF_TargetPixels.cpp`,
`NF_TargetPixels.h`, `NF_Target.h`, `README.MD`; not `ILB.png` or
`ILB.zip`) and `_NFLib/fx/NF_AlphaHyperbolic`, `NF_Blend`, `NF_Blur`,
`NF_MinMax`, `NF_Mult` (already here). `InkingLineBlurFilter.cpp` is
not in its vcxproj and is not compiled. Three units:
`NF_SelectPixels.cpp` and `NF_TargetPixels.cpp` share `ECTraits`,
`SPInfo` and `SelectPixels_Template`.

```
InkingLineBlur/InkingLineBlurFilter.h:5: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
InkingLineBlur/InkingLineBlurFilter.h:6: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
InkingLineBlur/InkingLineBlur.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
InkingLineBlur/InkingLineBlur.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
InkingLineBlur/InkingLineBlur.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
InkingLineBlur/InkingLineBlur.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
InkingLineBlur/InkingLineBlur.h:19: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
InkingLineBlur/InkingLineBlur.h:20: "..\_NFLib\fx\NF_blur.h" -> "../_NFLib/fx/NF_Blur.h"
InkingLineBlur/InkingLineBlur.h:21: "..\_NFLib\fx\NF_AlphaHyperbolic.h" -> "../_NFLib/fx/NF_AlphaHyperbolic.h"
InkingLineBlur/InkingLineBlur.h:22: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
InkingLineBlur/NF_SelectPixels.h:25: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
InkingLineBlur/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
InkingLineBlur/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
InkingLineBlur/NF_TargetPixels.h:25: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
```

Checked by eye: red and blue cels on white with 3 pixel black lines;
the line is grown (MinMax) and blurred and kept only on the target
colors (All: every opaque pixel), never on the line itself, so with
Blend with original the line bleeds into the red and blue cels and not
into the white when only those two are targets; MinMax 4 grows the
line by 4 to 5 pixels a side, 2 at scale 0.5.
FloodFill's ParamsSetup takes the default colors of its 16 slots from
an 8 entry table (`def_color[i % 16]`). gcc took that overrun as proof
the loop stops at 8 and dropped its end test, so setup ran off the
stack; `CMakeLists.txt` now builds F's code with
`-fno-aggressive-loop-optimizations`, and the loop runs as with MSVC.
The defaults of color9 to color16 are whatever sits past the table
(color9 changes from run to run), as in After Effects. Slots 2 to 16
are set up hidden; After Effects shows them through user changed param
when paintCount grows, which this host never sends.

### SelectiveColorBlur

Copied `SelectiveColorBlur/` (`SelectiveColorBlur.cpp`,
`SelectiveColorBlurSub.cpp`, `SelctColorCopy.cpp`,
`SelctCombineInput.cpp`, `SelectiveColorBlur.h`, `NF_Target.h`,
`README.MD`); its header includes `_NFLib/fx` headers carried for
FloodFill, but its vcxproj compiles none of their sources. Units:
`units/selectivecolorblur_unit_1.cpp` to `_4.cpp`, one per source (the
blur and the color copy both define `PixelTraits`).

```
SelectiveColorBlur/SelectiveColorBlur.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
SelectiveColorBlur/SelectiveColorBlur.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
SelectiveColorBlur/SelectiveColorBlur.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
SelectiveColorBlur/SelectiveColorBlur.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
SelectiveColorBlur/SelectiveColorBlur.h:19: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
SelectiveColorBlur/SelectiveColorBlur.h:20: "..\_NFLib\fx\NF_blur.h" -> "../_NFLib/fx/NF_Blur.h"
SelectiveColorBlur/SelectiveColorBlur.h:21: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
SelectiveColorBlur/SelectiveColorBlur.h:22: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
SelectiveColorBlur/SelectiveColorBlur.h:23: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
SelectiveColorBlur/SelectiveColorBlur.h:24: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
SelectiveColorBlur/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
SelectiveColorBlur/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
```

The blur only mixes target pixels with target pixels: one flat target
color looks unchanged; two target colors that touch (a base and its
shadow) get a soft border while lines and other colors stay crisp.
(Lines 19 to 27 of `TouchLines.h` sit in a comment.) Checked by eye:
the target mask finds the drawing's edges (luminance and alpha), lines
start on those edges straight along the angle or toward the center,
seed moves them, size halves at half scale. `value` is a chance per
edge pixel (value / 2000), so the default 1 draws almost nothing on a
small picture; that is the plugin's own scale.
## Light, glow and gradients (InnerFlare, InnerFlareRot, Star, Sputtering, Grain, TargetGrad, TargetGradRadical)

### InnerFlare

Files: `InnerFlare/` (`InnerFlare.cpp`, `InnerFlare.h`, `AlphaCopy.cpp`,
`AlphaCopy.h`, `AlphaCopyR.cpp`, `AlphaCopyR.h`, `NF_Target.h`,
`README.MD`); `_NFLib/fx/` `NF_AlphaHyperbolic`, `NF_Blend`, `NF_Blur`,
`NF_MinMax`, `NF_Mult` (`.cpp` and `.h`). Units
`innerflare_unit_1..3.cpp` (`AlphaCopy.cpp` and `AlphaCopyR.cpp` both
define `AlpheCopyInfo`).

```
InnerFlare/InnerFlare.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
InnerFlare/InnerFlare.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
InnerFlare/InnerFlare.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
InnerFlare/InnerFlare.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
InnerFlare/InnerFlare.h:19: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
InnerFlare/InnerFlare.h:20: "..\_NFLib\fx\NF_blur.h" -> "../_NFLib/fx/NF_Blur.h"
InnerFlare/InnerFlare.h:25: "..\_NFLib\fx\NF_AlphaHyperbolic.h" -> "../_NFLib/fx/NF_AlphaHyperbolic.h"
InnerFlare/InnerFlare.h:26: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
InnerFlare/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
InnerFlare/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
_NFLib/fx/NF_Blend.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
```

### InnerFlareRot

Files: `InnerFlareRot/` (`InnerFlareRot.cpp`, `InnerFlareRot.h`,
`AlphaCopyDD.cpp`, `AlphaCopyDD.h`, `AlphaCopyR.cpp`, `AlphaCopyR.h`,
`NF_Target.h`, `README.MD`); `_NFLib/fx/` `NF_ChannelBlur`,
`NF_ChannelMinMax` (`.cpp` and `.h`). Units `innerflarerot_unit_1..4.cpp`
(the AlphaCopy sources share `AlpheCopyInfo`; the Channel blur and
minmax share `BlurInfo` and `LineBuf` with the plain ones). Its README
names an `offset` parameter the code no longer has. `NF_ChannelBlur.cpp`
reports each blur pass through `OutputDebugStringA`, so every render
with blur prints `BoxBlurH8: err=0` lines on stderr.

```
InnerFlareRot/InnerFlareRot.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
InnerFlareRot/InnerFlareRot.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
InnerFlareRot/InnerFlareRot.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
InnerFlareRot/InnerFlareRot.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
InnerFlareRot/InnerFlareRot.h:19: "..\_NFLib\fx\NF_ChannelMinmax.h" -> "../_NFLib/fx/NF_ChannelMinMax.h"
InnerFlareRot/InnerFlareRot.h:20: "..\_NFLib\fx\NF_Channelblur.h" -> "../_NFLib/fx/NF_ChannelBlur.h"
InnerFlareRot/InnerFlareRot.h:25: "..\_NFLib\fx\NF_AlphaHyperbolic.h" -> "../_NFLib/fx/NF_AlphaHyperbolic.h"
InnerFlareRot/InnerFlareRot.h:26: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
InnerFlareRot/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
InnerFlareRot/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
```

### Star

Files: `Star/` (`Star.cpp`, `Star.h`, `StarMain.cpp`, `StarBlend.cpp`,
`Extract.cpp`, `DrawStar.cpp`, `NF_Target.h`, `README.MD`; not `_pool/`,
old zips). `DrawStar.cpp` is not compiled: F's project leaves it out.
Units `star_unit_1..2.cpp` (`Extract.cpp` and `StarBlend.cpp` both
define the inline `GetMaxChannel`, the same body). Its README names
Threshold low and high parameters the code no longer has. The output is
the stars alone on transparent, as F's code draws it (meant to be added
over the picture).

```
Star/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
Star/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
Star/Star.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
Star/Star.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
Star/Star.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
Star/Star.h:18: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
```

### Sputtering

Files: `Sputtering/` (`Sputtering.cpp`, `Sputtering.h`, `SputMain.cpp`,
`TargetWorld.cpp`, `TargetWorld.h`, `NF_Target.h`, `README.MD`; not the
`Data48/` pictures or `Sample.png`); `_NFLib/fx/NF_SpatUtils` (`.cpp`
and `.h`, which hold the built-in splat shapes). Units
`sputtering_unit_1..2.cpp` (`NF_Blend.cpp` and `NF_SpatUtils.cpp` both
specialise `PixelTraits`, with different members). The texture layer
parameter gets the host's empty layer, so the built-in splats are used,
as in After Effects with the layer set to None. Auto_Seed adds the frame
number, always 0 here, so only Seed changes the pattern. The target and
splat colors and Direction_Angle are added invisible and shown by F's
UPDATE_PARAMS_UI, which the host never sends: they take values but are
marked hidden. Spawn_Offset is not scaled down by F's code at preview
scale (the splats are).

```
Sputtering/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
Sputtering/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
Sputtering/Sputtering.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
Sputtering/Sputtering.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
Sputtering/Sputtering.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
Sputtering/Sputtering.h:17: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
Sputtering/Sputtering.h:18: "..\_NFLib\fx\NF_SpatUtils.h" -> "../_NFLib/fx/NF_SpatUtils.h"
Sputtering/Sputtering.h:22: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
Sputtering/Sputtering.h:23: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
Sputtering/Sputtering.h:24: "..\_NFLib\fx\NF_ChannelMinmax.h" -> "../_NFLib/fx/NF_ChannelMinMax.h"
Sputtering/Sputtering.h:25: "..\_NFLib\fx\NF_Blur.h" -> "../_NFLib/fx/NF_Blur.h"
Sputtering/Sputtering.h:26: "..\_NFLib\fx\NF_ChannelBlur.h" -> "../_NFLib/fx/NF_ChannelBlur.h"
Sputtering/Sputtering.h:27: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
Sputtering/Sputtering.h:28: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
Sputtering/Sputtering.h:29: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
Sputtering/Sputtering.h:30: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
Sputtering/Sputtering.h:31: "..\_NFLib\fx\NF_DrawAALine.h" -> "../_NFLib/fx/NF_DrawAALine.h"
Sputtering/Sputtering.h:36: "..\_NFLib\fx\NF_Blend.h" -> "../_NFLib/fx/NF_Blend.h"
Sputtering/TargetWorld.h:24: "..\_NFLib\NF_Utils.h" -> "../_NFLib/NF_Utils.h"
_NFLib/fx/NF_SpatUtils.h:24: "..\NF_Utils.h" -> "../NF_Utils.h"
```

(Lines 22 to 31 of `Sputtering.h` sit in a comment; patched anyway.)
