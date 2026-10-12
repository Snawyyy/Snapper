"""Krita side: rebuilds a Krita document from a Snapper doll folder.

The reverse of exporter.py, so a lost .kra, a doll made by someone
else, or drawings made in Snapper can be edited in Krita and exported
back without losing the rig. Matches src/io/ora_file.cpp, which does
the same without Krita.
"""

import json
import math
import os

from krita import Krita

try:
    from PyQt6.QtCore import QByteArray, QPointF
    from PyQt6.QtGui import QColor, QImage, QPainter, QPen
    from PyQt6.QtWidgets import QInputDialog, QMessageBox
except ImportError:  # Krita 5 ships PyQt5.
    from PyQt5.QtCore import QByteArray, QPointF
    from PyQt5.QtGui import QColor, QImage, QPainter, QPen
    from PyQt5.QtWidgets import QInputDialog, QMessageBox

from . import art_json
from .exporter import RIG_FILE, RIG_LAYER, library_folder

TITLE = "Import Snapper Doll"
ART_FILE = "art.json"
CROSS_HALF = 6


def dolls_in(folder):
    """Library doll folder names, sorted, without ".doll"."""
    if not os.path.isdir(folder):
        return []
    return sorted(name[:-len(".doll")] for name in os.listdir(folder)
                  if name.endswith(".doll") and
                  os.path.isfile(os.path.join(folder, name, ART_FILE)))


def read_json(path):
    if not os.path.isfile(path):
        return {}
    with open(path) as file:
        return json.load(file)


def growth(canvas_w, canvas_h, pieces):
    """How far the canvas grows on each side so every piece fits; even
    on both sides, so the middle (doll space's origin) stays put."""
    need_x = need_y = 0.0
    for piece in pieces:
        left = piece["position"][0] + canvas_w / 2.0
        top = piece["position"][1] + canvas_h / 2.0
        w, h = piece["size"]
        need_x = max(need_x, -left, left + w - canvas_w)
        need_y = max(need_y, -top, top + h - canvas_h)
    return int(math.ceil(need_x)), int(math.ceil(need_y))


def drawing_name(piece, index):
    names = piece.get("layer_names", [])
    if index < len(names) and names[index]:
        return names[index]
    return "%s %d" % (piece["name"], index + 1)


def pixel_bytes(image):
    """Krita's 8-bit RGBA is stored BGRA, which is QImage's ARGB32."""
    image = image.convertToFormat(QImage.Format.Format_ARGB32)
    bits = image.constBits()
    bits.setsize(image.sizeInBytes())
    return QByteArray(bytes(bits))


def paint_layer(document, name, image, x, y, visible):
    node = document.createNode(name, "paintlayer")
    if not image.isNull():
        node.setPixelData(pixel_bytes(image), x, y, image.width(),
                          image.height())
    node.setVisible(visible)
    return node


def rig_image(width, height, middle, art_pieces, rig):
    image = QImage(width, height, QImage.Format.Format_ARGB32)
    image.fill(QColor(0, 0, 0, 0))
    painter = QPainter(image)
    painter.setPen(QPen(QColor(230, 40, 60), 1.5))
    corners = {p["name"]: p["position"] for p in art_pieces}
    for piece in rig.get("pieces", []):
        corner = corners.get(piece.get("name"))
        if corner is None:
            continue
        pivot = piece.get("pivot", [0, 0])
        joint = QPointF(corner[0] + pivot[0] + middle[0],
                        corner[1] + pivot[1] + middle[1])
        painter.drawLine(joint - QPointF(CROSS_HALF, 0),
                         joint + QPointF(CROSS_HALF, 0))
        painter.drawLine(joint - QPointF(0, CROSS_HALF),
                         joint + QPointF(0, CROSS_HALF))
        painter.drawText(joint + QPointF(CROSS_HALF + 2, -CROSS_HALF),
                         piece["name"])
    painter.end()
    return image


class Tree:
    """Children of one group, bottom to top; a folder group stays open
    while the pieces after it share it, so the stack order is kept."""

    def __init__(self, name=None):
        self.name = name
        self.children = []  # Tree or a ready node.

    def group(self, name):
        last = self.children[-1] if self.children else None
        if not (isinstance(last, Tree) and last.name == name):
            last = Tree(name)
            self.children.append(last)
        return last

    def build(self, document):
        nodes = []
        for child in self.children:
            if isinstance(child, Tree):
                group = document.createGroupLayer(child.name)
                group.setChildNodes(child.build(document))
                nodes.append(group)
            else:
                nodes.append(child)
        return nodes


def problem(art):
    pieces = art.get("pieces", [])
    if not pieces:
        return "The doll has no pieces."
    if len(pieces) > art_json.MAX_PIECES:
        return "A doll holds at most %d pieces." % art_json.MAX_PIECES
    for piece in pieces:
        if not 1 <= len(piece.get("drawings", [])) <= art_json.MAX_DRAWINGS:
            return ("%s has %d drawings; a piece holds 1 to %d."
                    % (piece.get("name"), len(piece.get("drawings", [])),
                       art_json.MAX_DRAWINGS))
    return None


def build_document(name, folder):
    art = read_json(os.path.join(folder, ART_FILE))
    rig = read_json(os.path.join(folder, RIG_FILE))
    reason = problem(art)
    if reason:
        raise ValueError(reason)
    canvas_w, canvas_h = art["canvas"]
    pieces = art["pieces"]
    grow_x, grow_y = growth(canvas_w, canvas_h, pieces)
    width, height = canvas_w + 2 * grow_x, canvas_h + 2 * grow_y
    middle = (width / 2.0, height / 2.0)
    rig_defaults = {p.get("name"): p.get("default", -1)
                    for p in rig.get("pieces", [])}
    document = Krita.instance().createDocument(
        width, height, name, "RGBA", "U8", "", 72.0)
    root = Tree()
    for piece in pieces:
        into = root
        for group in piece.get("folder", []):
            into = into.group(group)
        x = int(round(piece["position"][0] + middle[0]))
        y = int(round(piece["position"][1] + middle[1]))
        default = rig_defaults.get(piece["name"], -1)
        if not 0 <= default < len(piece["drawings"]):
            default = piece.get("default", 0)
        layers = []
        for index, file in enumerate(piece["drawings"]):
            image = QImage(os.path.join(folder, file))
            single = len(piece["drawings"]) == 1
            layers.append(paint_layer(
                document, piece["name"] if single else
                drawing_name(piece, index), image, x, y,
                single or index == default))
        if len(layers) == 1:
            into.children.append(layers[0])
        else:
            group = document.createGroupLayer(piece["name"])
            group.setChildNodes(layers)
            into.children.append(group)
    root.children.append(paint_layer(
        document, RIG_LAYER, rig_image(width, height, middle, pieces, rig),
        0, 0, False))
    document.rootNode().setChildNodes(root.build(document))
    document.refreshProjection()
    return document


def run(parent):
    folder = library_folder()
    dolls = dolls_in(folder)
    if not dolls:
        QMessageBox.warning(parent, TITLE,
                            "There are no dolls in %s." % folder)
        return
    name, picked = QInputDialog.getItem(parent, TITLE, "Doll:", dolls, 0,
                                        False)
    if not picked:
        return
    try:
        document = build_document(name, os.path.join(folder,
                                                     name + ".doll"))
    except (OSError, ValueError, KeyError) as error:
        QMessageBox.warning(parent, TITLE, "Import failed: %s" % error)
        return
    Krita.instance().activeWindow().addView(document)
