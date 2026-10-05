"""Builds Snapper's art.json from piece boxes, without touching Krita.

Matches src/io/doll_json.cpp: doll space has its origin at the middle
of the Krita canvas, y down. Krita only ever writes art.json; the rig
lives in rig.json, which Snapper owns, so re-exporting never undoes
rigging.
"""

FORMAT = "snapper-art"
VERSION = 2
MAX_PIECES = 128  # kMaxDollPieces
MAX_DRAWINGS = 64  # kMaxPieceDrawings


def safe_name(name):
    """A piece name made safe for a file name."""
    keep = "".join(c if c.isalnum() or c in "-_" else "_" for c in name)
    return keep.strip("_") or "piece"


def drawing_names(piece_index, piece_name, count):
    """One PNG name per drawing, unique even when piece names clash."""
    assert count >= 1
    base = "%02d_%s" % (piece_index, safe_name(piece_name))
    return ["%s_%d.png" % (base, i) for i in range(count)]


def folder_name(name):
    safe = name.strip().replace("/", "_").replace("\\", "_")
    return (safe or "Doll") + ".doll"


def duplicate_names(names):
    """Names used more than once; Snapper matches the rig by name."""
    seen = set()
    twice = []
    for name in names:
        if name in seen and name not in twice:
            twice.append(name)
        seen.add(name)
    return twice


def build(canvas_w, canvas_h, pieces):
    """pieces: dicts with name, x, y, w, h, drawings, default; bottom to
    top, in canvas pixels."""
    assert canvas_w > 0 and canvas_h > 0
    assert len(pieces) <= MAX_PIECES
    out = []
    for piece in pieces:
        assert 1 <= len(piece["drawings"]) <= MAX_DRAWINGS
        assert 0 <= piece["default"] < len(piece["drawings"])
        out.append({
            "name": piece["name"],
            "drawings": piece["drawings"],
            "default": piece["default"],
            "position": [piece["x"] - canvas_w / 2.0,
                         piece["y"] - canvas_h / 2.0],
            "size": [piece["w"], piece["h"]],
        })
    return {"format": FORMAT, "version": VERSION,
            "canvas": [canvas_w, canvas_h], "pieces": out}


if __name__ == "__main__":
    art = build(100, 80, [
        {"name": "body", "x": 0, "y": 0, "w": 100, "h": 80,
         "drawings": ["00_body_0.png"], "default": 0},
        {"name": "head", "x": 40, "y": 10, "w": 20, "h": 10,
         "drawings": ["01_head_0.png", "01_head_1.png"], "default": 1},
    ])
    body, head = art["pieces"]
    assert body["position"] == [-50.0, -40.0]
    assert head["position"] == [-10.0, -30.0]
    assert head["default"] == 1 and head["size"] == [20, 10]
    assert art["format"] == FORMAT and art["version"] == VERSION
    assert drawing_names(3, "Left Arm!", 2) == ["03_Left_Arm_0.png",
                                                "03_Left_Arm_1.png"]
    assert safe_name("!!") == "piece"
    assert duplicate_names(["a", "b", "a", "a"]) == ["a"]
    assert folder_name(" a/b ") == "a_b.doll" and folder_name("") == "Doll.doll"
    print("ok")
