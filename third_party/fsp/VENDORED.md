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

### Grain

Files: `Grain/` (`Grain.cpp`, `Grain.h`, `NF_Target.h`, `README.MD`);
`_NFLib/fx/` `NF_Noise`, `NF_Spat_Data` (`.cpp` and `.h`). Units
`grain_unit_1..2.cpp`. `NF_Noise.cpp` and `NF_Spat_Data.cpp` both define
an inline `hash_01` with different formulas; in two units of one
namespace the linker would keep only one, so `grain_unit_2.cpp` renames
`NF_Spat_Data.cpp`'s by macro (`fsp_grain_spat_hash_01`), as the entry
points are renamed. Both hashes overflow signed ints and count on them
wrapping as under MSVC; gcc at -O2 folded `NF_Spat_Data`'s to one value,
putting every accent grain on one spot, so `fsp_vendor` is now built
with `-fwrapv` (CMakeLists.txt). autoSeed adds the frame number, always
0 here, so only seed changes the pattern. Accent grains keep their size
at preview scale (F's code does not scale them); the grain size does.

```
Grain/Grain.h:11: "..\_NFLib\AE_SDK.h" -> "../_NFLib/AE_SDK.h"
Grain/Grain.h:15: "..\_NFLib\NF_AE.h" -> "../_NFLib/NF_AE.h"
Grain/Grain.h:16: "..\_NFLib\NF_ParamsSetup.h" -> "../_NFLib/NF_ParamsSetup.h"
Grain/Grain.h:19: "..\_NFLib\fx\NF_Mult.h" -> "../_NFLib/fx/NF_Mult.h"
Grain/Grain.h:20: "..\_NFLib\fx\NF_Minmax.h" -> "../_NFLib/fx/NF_MinMax.h"
Grain/Grain.h:21: "..\_NFLib\fx\NF_blur.h" -> "../_NFLib/fx/NF_Blur.h"
Grain/Grain.h:22: "..\_NFLib\fx\debug_font.h" -> "../_NFLib/fx/debug_font.h"
Grain/Grain.h:23: "..\_NFLib\fx\NF_Paint.h" -> "../_NFLib/fx/NF_Paint.h"
Grain/Grain.h:24: "..\_NFLib\fx\NF_Draw.h" -> "../_NFLib/fx/NF_Draw.h"
Grain/Grain.h:26: "..\_NFLib\fx\NF_Noise.h" -> "../_NFLib/fx/NF_Noise.h"
Grain/Grain.h:27: "..\_NFLib\fx\NF_Spat_Data.h" -> "../_NFLib/fx/NF_Spat_Data.h"
Grain/NF_Target.h:7: "..\_NFLib\NF_CATEGORY.h" -> "../_NFLib/NF_CATEGORY.h"
Grain/NF_Target.h:8: "..\_NFLib\NF_Version.h" -> "../_NFLib/NF_Version.h"
_NFLib/fx/NF_Noise.h:26: "..\NF_Utils.h" -> "../NF_Utils.h"
```

(Lines 19 to 24 of `Grain.h` sit in a comment; patched anyway.)
