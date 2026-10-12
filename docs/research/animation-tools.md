# 2D animation tools: Moho and peers

2026-10-12, Eitan Katayev

A study of Moho and three peer tools, aimed at finding the animation features worth reverse engineering.

## Overview

Moho is the cutout tool with the richest character rigging of the four. Spine is the most focused on game runtimes, Toon Boom Harmony is the studio standard, and DragonBones is the open-runtime option.

| Tool | Maker | Core approach | Built for | Price and license |
| --- | --- | --- | --- | --- |
| Moho | Lost Marble | Bones, Smart Bones, switch layers | Cutout characters, lip sync | Proprietary, trial available |
| Spine | Esoteric Software | Weighted meshes, bones, IK | Game characters and runtimes | Free trial; paid to save and export |
| Toon Boom Harmony | Toon Boom | Drawing plus cut-out rigging | TV and film studios | Subscription |
| DragonBones | Nezha Interactive | Skeletal animation, one export many runtimes | Game developers | Editor terms not confirmed; Haxe runtime MIT |

## Moho

Moho is a 2D cutout animation tool built around bones. It was first released in 1999 by Lost Marble as Moho, was sold as Anime Studio from 2007, and returned to the Moho name in 2016.

- Bones and rigging: Smart Bones control joints and facial expressions, and one bone can turn a whole body. It also has forward and inverse kinematics, advanced and delayed constraints, Vitruvian Bones (switch between drawings and bone sets), pin bones and shy bones.
- Layers: switch layers swap drawings on a timeline, and their interpolation for nested layers arrived in version 12.4. Switch layers were added in version 2.2 and enhanced in 9.5.
- Mesh and warp: automatic mesh creation, quad meshes (version 13.5) and smart warp for bending shapes.
- Lip sync: import a voice track and the mouth animates automatically, or build it by hand with the switch selection window.
- Timeline and graph editor: interpolation modes, cycles, and a graph mode for editing animation curves directly.
- Physics: a physics engine, particles, gravity, dynamics and wind dynamics for loose parts; the Moho pages did not name hair or cloth tools.
- Export: Full HD video, web, TV and film output, and game engine export to Unity, Unreal, Godot and Blender (free for Moho 14 owners).

What is most useful to copy: Smart Bones (one control drives many joints), the switch selection window for lip sync, and the graph editor.

Not confirmed: the Moho pages opened did not mention Auto-Smooth, Auto-Rig or motion presets, so those are left out until a source confirms them.

## Spine

Spine is a 2D skeletal animation editor by Esoteric Software, aimed at game developers. Its main idea is that images attach to bones, and the animation moves the bones and the mesh follows.

- Bones and skinning: mesh vertices attach to several bones with weights, so the image bends smoothly as the bones move.
- Mesh attachments and free-form deformation: a polygon replaces the plain rectangle, and single vertices can be moved to stretch, squash or bend the image.
- Skins: sets of attachments that swap, so one animation can drive characters with different looks.
- Inverse kinematics: a Pose tool uses multi-bone IK to pose a whole limb or skeleton quickly.
- Path constraints: bones follow a Bezier spline, which lets many bones move in a controlled line.
- Dopesheet and graph: a timeline for fine timing, and Bezier curves to shape the easing between keys.
- Texture packing: packs images into an atlas with white space stripping and rotation.
- Runtimes: export data and play it in game engines, with official runtimes for 19 toolkits and third-party runtimes beyond that.

What is most useful to copy: weighted skinning with a live mesh, and the Pose tool that solves multi-bone IK in one drag.

Not confirmed: the pages opened did not cover slots, events or transform constraints in detail, though the Spine 4.3 announcement names transform constraints. Pricing tiers were not found on the pages opened; a free trial exists.

## Toon Boom Harmony

Toon Boom Harmony is a professional studio package for 2D animation. It covers traditional frame-by-frame drawing and cut-out rigging in one program, and it is aimed at TV and film studios.

- Drawing: brush, pencil, contour editor and a Close Gap tool that seals small outline gaps.
- Paint and swatches: editing a palette swatch updates every area painted with it.
- Paperless animation: onion skin, easy flipping, thumbnail mode and key-drawing marking.
- Morphing: generates inbetweens between two vector drawings, with ease and morph hints.
- Cut-out rigging: pegs, parent and child layer hierarchies, a master peg and Z-axis nudging for layer order.
- Deformers: bone, curve, envelope and game bone deformers.
- Cut-out animation: transform and inverse kinematics tools, ease for multiple parameters, and drawing substitution to swap mouths, hands and eyelids from a library.
- Lip sync: sound import, automatic lip-sync detection, and manual lip-sync animation.
- Timeline and Xsheet: a timeline for digital and cut-out work, and an exposure sheet for traditional timing.
- Camera and effects: camera layers, camera keyframes on a peg, mattes and a node view for compositing.

What is most useful to copy: drawing substitution from a library for mouths and hands, and automatic lip-sync detection from imported audio.

Not confirmed: the Toon Boom site blocked automated reading, so this section uses the Harmony 16 Premium getting started guide. Pricing is a subscription; the exact tiers were not checked.

## DragonBones

DragonBones is a 2D skeletal animation platform from Nezha Interactive, made for game developers. Its authoring tool is DragonBonesWeb, and it ships runtime libraries for common game engines. The project started in October 2012.

- Skeletal animation: one animation is made once and reused across platforms, which is its core promise.
- Runtimes: libraries load the exported data into engines. The Haxe runtime, for example, is MIT licensed and installs through haxelib.
- Dynamic comics: a second solution for rich media, besides skeletal animation.

What is most useful to copy: the one-export, many-runtimes split, which keeps the editor separate from the code that plays the animation.

Not confirmed: the pages opened did not list its bones, mesh, IK, slot or blending features in detail, and the editor's license was not confirmed. Check the DragonBones docs before relying on those points.

## Helpful tools to reverse engineer

Ranked by how much each would help a puppet-style animation tool such as Snapper, where a character is a set of cut out parts that bend and move.

| Rank | Feature | Tool | What it does | Why it helps |
| --- | --- | --- | --- | --- |
| 1 | Smart Bones | Moho | One control drives many joints, or a whole body turn | Fewer controls to pose a doll |
| 2 | Weighted skinning with FFD | Spine | Mesh vertices follow several bones, and single vertices can be moved | Bending a part smoothly, the core of warp dots |
| 3 | Multi-bone IK Pose tool | Spine | One drag poses a whole chain | Quick posing of limbs |
| 4 | Lip sync from audio | Moho, Toon Boom Harmony | Mouth shapes set from a voice track | Speech for dolls |
| 5 | Drawing substitution | Toon Boom Harmony | Swap mouths, hands and eyelids from a library | Swapping parts within a doll |
| 6 | Skins | Spine | Swap sets of attachments on one rig | One rig, many looks |
| 7 | Path constraints | Spine | Bones follow a Bezier spline | Controlled motion for chains such as hair |
| 8 | Graph editor and dopesheet | Moho, Spine | Bezier curves shape easing between keys | Timing control per property |
| 9 | Physics: gravity, dynamics, wind | Moho | Parts move with simulated forces | Jelly and loose parts |
| 10 | Switch layers | Moho | Swap drawings on a timeline, interpolated for nested layers | Frame-based swaps |

Start with rows 1 to 3: they map directly onto the pose and warp work, and the sources describe them clearly.

## Sources

- [Moho features page](https://moho.lostmarble.com/pages/features), read for Moho's rigging, lip sync, timeline and export features
- [Moho on Wikipedia](https://en.wikipedia.org/wiki/Moho_(software)), read for history and version dates
- [Spine: What is Spine?](https://us.esotericsoftware.com/spine-in-depth), read for Spine's bones, skinning, IK, path constraints and atlas packing
- [Spine home page](https://esotericsoftware.com/), read for mesh deformation, the Pose tool and the trial
- [Toon Boom Harmony 16 Premium Getting Started Guide](https://docs-stg.toonboom.com/download/harmony/16.0/premium/Harmony_16_0_Premium_Getting_Started_Guide.pdf), read for drawing, cut-out, deformers and lip sync
- [DragonBones home page](https://dragonbones.github.io/), read for the company, the editor and runtimes
- [DragonBones Haxe runtime on GitHub](https://github.com/openfl/dragonbones), read for the MIT license of the runtime

Not opened: the Toon Boom product site, which blocked automated reading, and the DragonBones license terms.
