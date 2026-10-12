# Smooth warp

Status: built, first pass (2026-10-12); owner tests by eye. Inspired by the Pixel Edge Upscaler test page (xBR and RotSprite).

The first pass: each doll's rig has a look (Crisp, Smooth, Smooth flat) and a Hard edges switch, set in the Rig tab's Look box (three tiles and a tick box, one undo step each through RigManager), and each piece a Keep crisp tick in its settings. Saved in rig.json and kept on re-import; a rig without a look opens Crisp, a doll with no rig.json yet starts Smooth. A smooth drawing is upscaled once (Scale2x, the simplest of the xBR family, applied twice for 4x, once for 2x) when first drawn and kept in the renderer's WarpCache next to its bends, within its byte budget, least recently used dropped first. Warps blend between the upscaled pixels (bilinear); turned and scaled pieces draw the upscaled copy with the painter's smooth scaling, so both moves share the look. Smooth flat keeps fully covered pixels on the drawing's own colors; Hard edges keeps or drops edge pixels whole. Bends are made up to twice the drawing's size when zoomed in. Hold backslash on the stage to see everything Crisp. Hit tests, meshes, colliders and outlines still read the original drawing.

Not built yet: real xBR (Scale2x stands in), the live bent preview on each tile, upscaling in the background, the on-disk cache next to the doll, a draft look while dragging, the export dialog's look override, and Smooth flat for pieces that are only turned or scaled (they draw like Smooth). "Very large" means a side past 2048 pixels (upscaled 2x at most), since the renderer does not know the canvas.

Needs: nothing.

Warped, turned and scaled pieces should look like the smoothed anime frame, not like blown-up pixel brush strokes. Today the warp reads the nearest pixel (`render/warp_raster.cpp`), so a stretched part shows big square steps. The fix: upscale each drawing once with xBR when the doll loads, keep it in the warp cache, and have the warp blend between pixels instead of picking one.

## The look

- Three looks per doll: Crisp (today, for real pixel art), Smooth (xBR upscale plus blended reading, the default for new dolls) and Smooth flat (Smooth, then inner pixels snapped back to the piece's own colors so fills stay flat and only the edges soften).
- One look for everything that moves a drawing: warp, turn, scale and skew. A part should not look smooth while bending and jagged while only turning.
- Edges: soft edges by default (blended into what is behind), with a Hard edges option for a cutout look.
- Smooth at every zoom: zooming the stage in shows smooth curves, not the upscaled steps.

## Where the switch is

- In the Rig tab, the doll's settings box: a three-tile picker (Crisp, Smooth, Smooth flat) with a live preview of a bent sample of the doll's own art on each tile.
- Per piece override in the piece settings, for the odd part that must stay crisp (a pixel-art screen on a character's chest).
- Hold a key (proposed: backslash) to see the stage in Crisp while held, and let go to go back, to compare without changing the setting.
- Saved in the rig, so the look travels with the doll.

## Speed

- Each drawing is upscaled once, in the background, when the doll loads. Until it is ready the piece draws Crisp, then swaps in with no jump in place or size.
- Upscaled drawings are cached on disk next to the doll, keyed by the drawing's content, so opening the project again is instant and a Krita re-export only redoes the changed drawings.
- While dragging a dial, a handle or playing in preview, the stage may draw at draft quality; when the drag ends it redraws at full quality. The same rule the preview already follows for smaller scale.
- Memory: only drawings in use are upscaled, within the existing cache budget; the least recently used are dropped first.

## Export

- Export always uses full quality, whatever the preview is doing.
- The export dialog shows the look in use and can override it for one export (for a crisp pixel-art version of the same reel).

## What must not change

- Clicking, hover and picking keep using the original drawing, so what you click stays exactly what you clicked before.
- The shape mesh, colliders and outlines are still built from the original drawing, so rigs and physics behave the same.
- Placement: a smooth piece sits on exactly the same spot as the crisp one, pixel for pixel at rest.

## Edge cases

- A drawing made or changed inside Snapper ([Draw on a piece](draw-on-a-piece.md)) is upscaled again when saved.
- Very large drawings (bigger than the canvas) upscale at 2x instead of 4x to keep memory in check, with the same look.
- Old projects open in Crisp so nothing changes under the owner's feet; new dolls start in Smooth.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| Backslash (hold) | Stage | Show Crisp while held, to compare with Smooth |

## Open questions

- [x] Should new dolls start Smooth or Smooth flat? First pass: Smooth, as the doc proposes.

## Not in the first version

- Moving the renderer to the GPU. The renderer is built so that swap changes nothing else, but the CPU is enough for flat-color dolls today.
