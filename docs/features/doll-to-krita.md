# Doll to Krita

Status: planned.

A doll folder holds PNGs and two JSON files, not the Krita file it came from. Doll to Krita rebuilds a Krita file from the folder, with every piece as a group and every drawing as a named layer, so a lost .kra, a doll made by someone else, or drawings made in Snapper can be edited in Krita and sent back without losing the rig.

## First, the exporter must keep names

- Today the Krita exporter (`tools/krita`) names each drawing file by position ("02_head_1.png") and drops the Krita layer names and the folder groups. A round trip needs them.
- The exporter adds to art.json: each drawing's layer name, and each piece's folder path (the groups holding groups). Old dolls without them still load; their names become "head 1", "head 2".
- [Smart Dials](smart-dials.md) and [Skins](skins.md) then refer to drawings by these names, which is what keeps them working after a re-export.

## Two ways to make the Krita file

- In Krita (the main way): the Snapper plugin gets an Import Snapper doll action. Pick a doll from the library and it builds a new Krita document with Krita's own tools, so names, groups, visibility and positions come out exact. Save it as .kra from there.
- From Snapper (no plugin needed): Export doll to Krita writes an OpenRaster file (.ora), which Krita opens with groups and layer names. The user saves it as .kra in Krita.

## What the Krita file holds

- Canvas: the doll's Krita canvas size, at 8-bit sRGB like the export, pixels copied exactly with no resampling.
- Each piece with several drawings is a group; each drawing a layer inside it, named as in Krita. The default drawing is visible, the others hidden, the same rule the exporter reads.
- A piece with one drawing is a plain layer. Folder groups come back where they were. Order bottom to top as the doll draws.
- Snapper drawings are included with a color label (Krita's layer color tags) so they are easy to find; skin drawings keep their "skin/drawing" names.
- A hidden "Snapper rig" layer on top shows each joint as a small cross with its name, for reference while redrawing. The exporter ignores it.
- Pieces a skin hides, and joint circles, are Snapper's and are not drawn into the file.

## Getting there

- Right click a cast card: Open in Krita. Also in the Rig tab doll menu and File > Export doll to Krita.
- The first time it asks where to save (default: next to the doll library, "doll.ora"). The path is remembered in the rig, so Open in Krita next time opens that same file, and only offers to rebuild it if the doll changed since.
- If Krita is found on the system it opens straight away; otherwise the file is shown in the folder and a setting takes Krita's path.
- Done shows a toast with the file name and a Show in folder button.

## Coming back from Krita

- Exporting from Krita again goes to the same doll folder, matched by name, so the rig, dials, skins and keys stay, as today.
- A Snapper drawing now also coming from Krita (same name in the same piece): Snapper asks once whether to use Krita's version from now on and retire its own copy.
- New layers in Krita become new drawings; removed ones are reported, never silently dropped, the same as pieces today.

## Edge cases

- Layer names with "/" that are not skins ("left/right arm"): the import offers to treat a prefix as a skin only when it appears on several pieces.
- Pieces bigger than the canvas are kept whole; the Krita canvas grows to fit and the offset is stored so the export comes back to the same place.
- The 64 drawings and 128 pieces limits apply both ways and are checked before writing, with a message naming the piece.
- [Objects](objects.md) work the same way.

## Not in the first version

- Writing .kra directly from Snapper (its format is far heavier than OpenRaster), Krita animation frames and vector layers.
