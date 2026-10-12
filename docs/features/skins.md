# Skins

Status: planned. Inspired by Spine's skins.

A skin is a named look for a doll: for each piece it touches, which drawing shows. Picking a skin swaps all of them at once (school uniform, pajamas, angry face), so one rig, one set of keys and one set of dials serve every outfit, without making a second doll.

## Making a skin (Rig tab)

- A Skins box in the Rig tab, shown when a doll is picked. A "+" tile makes a skin from the doll as it looks now, named in place.
- To change a skin: pick it, then click pieces on the stage and swap their drawing with the Drawing picker or the wheel; the skin records every piece you change. Pieces you never touch are not part of it.
- A skin can hide a piece (a hat off) with a "None" drawing, and show a piece the default hides.
- Right click a skin tile: Rename, Duplicate, Update from current look, Delete. Delete is greyed while shots key it, with a tooltip naming them.
- Each tile shows a small picture of the doll wearing it, redrawn when the skin changes.

## Krita naming, so skins build themselves

- In Krita, name a child layer "skin/drawing" ("pajamas/arm", "pajamas/mouth O"). On load, Snapper offers to make a skin for each prefix it finds, mapping each drawing to the plain one it replaces. This needs the exporter to keep layer names ([Doll to Krita](doll-to-krita.md)).
- That map is what makes dials work in every skin: a [Smart Dial](smart-dials.md) stop that shows "mouth O" shows "pajamas/mouth O" while the pajamas skin is on, when that drawing exists, and the plain one otherwise.
- Snapper drawings ([Draw on a piece](draw-on-a-piece.md)) can be saved into a skin the same way: Done offers "Also for skin: pajamas".

## Stacking skins

- Skins stack: a body skin (pajamas) and a face skin (angry) can be on together. Each changes only its own pieces.
- If two skins touch the same piece, the one higher in the Skins box wins. Drag tiles to reorder.
- A hand swap on a piece in the Pose tab beats every skin for that key, so a single odd frame needs no new skin.

## Using skins (Pose tab)

- A Skins row in the Pose box when the whole doll is picked: tiles with the doll pictures; click to put one on, click again to take it off; Shift click to stack another.
- Keyed with the doll like the rest of the pose, holding until the next key. A Skins row under the doll in the timeline shows each change as a chip with the skin name, so an outfit change is easy to find and drag.
- Several dolls picked: a skin with the same name goes on all of them; dolls without it are greyed with a tooltip.
- Cast cards: a skin dot row on each doll card; dragging the doll onto the stage brings it in wearing the picked skin, which becomes that layer's starting skin.

## Colors that follow the skin

- [Joint circles](joint-snap.md) can take a color per skin (a pajama sleeve elbow is not a uniform elbow). If a skin sets none, the circle measures the color again from the swapped drawing.
- The floating palette shows the colors of the skin's drawings while that skin is on.

## Undo, saving and Krita

- Every skin change, on or off, is one undo step, named ("Undo Skin pajamas").
- Skins save in the rig file, with drawings named by their Krita layer name, so a Krita re-export keeps them. A drawing a skin needs that is gone is reported and that piece falls back to its default, never silently dropped.
- Mirror copies a skin's choices to twin pieces.

## Edge cases

- A piece added in Krita after a skin was made: shows its default in that skin until the skin is updated.
- Bend mesh: skins only use drawings the piece already has, so the mesh, built from all of them, already covers every skin.

## Hotkeys

| Key | Where | Does |
| --- | --- | --- |
| Shift+[ and Shift+] | Doll picked | Previous and next skin |

## Not in the first version

- Skins that change the rig itself (pivots, piece sizes, extra pieces), and mixing two skins' colors on one piece.
