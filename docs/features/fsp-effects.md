# F's Plugins in Snapper

Status: design (2026-10-13). Goal from the owner: take the open source anime compositing tools and make them work inside Snapper, instead of inventing look-alikes. The Comp tab's first nine effects were written from the tools' descriptions and are approximations; this brings the real code in.

## What can and cannot come in

- F's Plugins (https://github.com/bryful/F-s-PluginsProjects, bryful, MIT, source on GitHub, vendored from commit db6dad39 of 2026-05-04, recorded in `third_party/fsp/VENDORED.md`): about 50 After Effects plugins in C++. Their pixel code can run in Snapper.
- OLM OpenTools (Apache 2.0): Windows After Effects binaries only, no source. They cannot be loaded or ported. Our own rebuilds of them stay marked as approximations.
- Loading arbitrary plugin files (Frei0r, OpenFX) is a later job; the F's route below leaves room for it and does not need it.

## How: run the real plugin code against a fake host

A plugin is `EffectMain(cmd, in_data, out_data, params, output)`. `PF_Cmd_PARAMS_SETUP` declares its parameters, `PF_Cmd_RENDER` draws. Everything it calls (`PF_ADD_SLIDER`, `NF_AE` helpers, the Iterate suites, `PF_EffectWorld`) is a thin layer over After Effects. We supply a small stand-in for that layer, so F's own `ParamsSetup`, `GetParams`, `Exec` and the `*Sub.cpp` pixel loops compile and run unchanged (apart from the patches listed in `VENDORED.md`).

1. `third_party/fsp/` holds the vendored sources (MIT licence file kept, credit in the app's About/Help). Patches are only: include paths (`\` to `/`, filename case), and anything the build needs; each is logged in `VENDORED.md`. Never rewrite an algorithm to "improve" it.
2. `third_party/fsp/shim/` is the stand-in host, at the level of the After Effects SDK: one-line headers under the SDK's file names (`AE_Effect.h`, `Param_Utils.h`, `AEGP_SuiteHandler.h` ...) lead to `shim/include/fsp_shim/`, which declares the types (`PF_Pixel8/16/Float`, `PF_EffectWorld`, `PF_InData`, `PF_OutData`, `PF_ParamDef` ...), the `PF_ADD_*` and helper macros, the suites (`AEGP_SuiteHandler`, `AEFX_SuiteScoper`, Iterate, World, WorldTransform, FillMatte, Handle, ANSI) and the host callbacks; `shim/src/` implements them. F's own `NF_AE.h` runs unchanged on top, so every `NF_AE` member works as in After Effects. The Iterate suites run on up to 16 threads. Anything not carried yet fails loudly (prints `fsp: ... is not supported`, the render returns an error). Compatible in behaviour, not copied from Adobe's SDK. `third_party/fsp/VENDORED.md` lists what works and what does not.
3. Each plugin is compiled in a namespace of its own (`namespace fsp_rimfill { #include ... }`, `units/<prefix>_unit.cpp`) with `EffectMain` and `PluginDataEntryFunction2` renamed per plugin by macro, so two plugins' same-named helpers never merge at link time. A plugin whose sources clash in one unit (file-local helpers of the same name) is split into several units in the same namespace. Compiled with warnings off (`-w -fpermissive`), `-O2` in Debug: it is not our code. It sits outside `src/`, so the linter and the layer check do not read it. `registry.cpp` lists the plugins, one line each.
4. Between the two sits a plain C interface, `third_party/fsp/include/fsp_host.h` (no After Effects types): plugin count, ids, names, per parameter name, type, range, slider range, default, popup entries and slot, and `fsp_host_render(plugin, w, h, argb_in, argb_out, values, count, scale)`. The only thing the rest of Snapper sees is `src/fsp/fsp_effects.h` over it: the list of plugins, each one's parameters (read by running its own `PARAMS_SETUP`, never typed in by hand), and `RunFspEffect(info, values, image, scale)`. `src/fsp` is our code and follows Snawy's Law. Point values are kept in percent of the picture (After Effects' own defaults are percent), turned into pixels of the picture at each run.
5. The Comp tab gets one new kind, `CompKind::kFsp`, with the plugin's id and its values on the effect; the panel builds its rows from the plugin's parameters (slider, float, angle, toggle, choice, color, point). Targeting, blend modes and keys on numbers work as for the other effects.

The pixel layout After Effects uses is straight-alpha ARGB bytes. The wrapper converts to and from `QImage` once per run.

## Which plugins, in order

Tier 1, the ones the anime workflow needs: Outline, RimFill, Thin, ColorSwitch, SpeedLines, TouchLines, InnerFlare and InnerFlareRot, LineExtraction, WhiteKeying, AlphaFill, FloodFill, SelectiveColorBlur, CellLineEraser, BurstLines, RadialLines, MuzzleFlash, InkingLineBlur, Grain, Star, Sputtering, TargetGrad and TargetGradRadical, Posterization, MaxBlur, MinMax, EdgeLine and the other `Cell*` ones.
Tier 2: Channel, Colorize, Draw, Filter, Noise groups (Emboss, Chroma, Threshold, RGBShifter, Mosaic, Gradient ...).
Skipped: anything that needs time across frames or other layers from the host (CameraShake, expression-driven ones), until the shim can answer those.

## Done means

For each plugin: it appears in the Add menu under its After Effects name, its parameters and ranges are the plugin's own, a run on a test picture gives what the plugin's description says, pixel widths follow the preview scale, and nothing is hard-coded per plugin outside its own wrapper line in the registry.

## Checking by eye

`tools/fsp_render` (a development tool, not a test) runs one plugin over a PNG with chosen values and writes a PNG, so a port can be looked at without the app. Build it with `cmake --build --preset debug --target fsp_render`; `build/fsp_render` lists the plugins and their parameters, `build/fsp_render RimFill in.png out.png width=6 CustomColor=#00c000 scale=0.5` renders.

## Status (2026-10-13)

- Built: the stand-in host, the C interface, `src/fsp/fsp_effects.cpp`, `fsp_render`, and RimFill, checked by eye (custom and adjacent color, white as transparent, half scale).
- All 26 tier 1 plugins compiled, linked and set up against the host in a trial and rendered with their defaults without an error; only RimFill is carried so far. To add one, follow "Adding a plugin" in `third_party/fsp/VENDORED.md`.
- Not built yet: the Comp tab's `CompKind::kFsp` and its panel, credit in About/Help, a time or frame number for plugins that animate (Grain's seed: every run is frame 0 now).
