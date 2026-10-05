"""Krita side: turns the open document into a Snapper doll."""

import contextlib
import json
import os
import shutil

from krita import Extension, Krita

try:
    from PyQt6.QtCore import QRect, QStandardPaths
    from PyQt6.QtGui import QImage
    from PyQt6.QtWidgets import QMessageBox
except ImportError:  # Krita 5 ships PyQt5.
    from PyQt5.QtCore import QRect, QStandardPaths
    from PyQt5.QtGui import QImage
    from PyQt5.QtWidgets import QMessageBox

from . import art_json

TITLE = "Export Snapper Doll"
MAX_NODES = 4096  # Bounds the layer walk.
NOT_PAINT = {"grouplayer", "transparencymask", "filtermask",
             "transformmask", "selectionmask", "colorizemask"}
RIG_FILE = "rig.json"


def library_folder():
    # Krita's data folder sits beside Snapper's on every platform: Qt
    # puts both under <data>/<org>/<app>, and Snapper's are both
    # "Snapper".
    krita_data = QStandardPaths.writableLocation(
        QStandardPaths.StandardLocation.AppDataLocation)
    return os.path.join(os.path.dirname(krita_data), "Snapper", "Snapper",
                        "dolls")


def is_paint(node):
    return node.type() not in NOT_PAINT


def pieces_of(document):
    """(name, [drawing nodes], default index), bottom to top.

    A visible group holding only layers is one piece whose layers,
    hidden ones too, are its drawings; the visible one is the default.
    A group holding groups is just a folder. A visible plain layer is a
    piece with one drawing.
    """
    pieces = []
    stack = list(reversed(document.topLevelNodes()))
    for _ in range(MAX_NODES):
        if not stack:
            break
        node = stack.pop()
        if not node.visible():
            continue
        children = [c for c in node.childNodes()
                    if c.type() == "grouplayer" or is_paint(c)]
        if node.type() == "grouplayer":
            if any(c.type() == "grouplayer" for c in children):
                stack.extend(reversed(children))
            elif children:
                visible = [i for i, c in enumerate(children) if c.visible()]
                pieces.append((node.name(), children,
                               visible[-1] if visible else 0))
        elif is_paint(node) and not node.bounds().isEmpty():
            pieces.append((node.name(), [node], 0))
    return pieces


def shared_box(drawings):
    """One box around every drawing, so swaps line up."""
    box = QRect()
    for node in drawings:
        box = box.united(node.bounds())
    return box


@contextlib.contextmanager
def shown(node, document):
    """Shows node while its pixels are read, then hides it again, so
    hidden swap drawings export with their masks applied."""
    was_visible = node.visible()
    if not was_visible:
        node.setVisible(True)
        document.waitForDone()
    try:
        yield
    finally:
        if not was_visible:
            node.setVisible(False)
            document.refreshProjection()


def save_png(node, box, path, document):
    with shown(node, document):
        data = node.projectionPixelData(box.x(), box.y(), box.width(),
                                        box.height())
    # Krita's 8-bit RGBA is stored BGRA, which is QImage's ARGB32.
    image = QImage(bytes(data), box.width(), box.height(),
                   QImage.Format.Format_ARGB32).copy()
    return image.save(path, "PNG")


def problem(document):
    """Why the document can't be exported, or None."""
    if document is None:
        return "Open a document first."
    if document.colorModel() != "RGBA" or document.colorDepth() != "U8":
        return ("Snapper dolls are 8-bit RGB. Convert with Image > "
                "Convert Image Color Space to RGB/Alpha, 8-bit.")
    pieces = pieces_of(document)
    if not pieces:
        return "There are no visible layers with paint on them."
    if len(pieces) > art_json.MAX_PIECES:
        return ("A doll holds at most %d pieces; this document has %d."
                % (art_json.MAX_PIECES, len(pieces)))
    crowded = [n for n, d, _ in pieces if len(d) > art_json.MAX_DRAWINGS]
    if crowded:
        return ("A piece holds at most %d drawings; %s has more."
                % (art_json.MAX_DRAWINGS, crowded[0]))
    twice = art_json.duplicate_names([n for n, _, _ in pieces])
    if twice:
        return ("Two pieces are called \"%s\". Snapper keeps rigs by "
                "piece name, so rename one." % twice[0])
    return None


def write_doll(document, folder):
    os.makedirs(folder)
    described = []
    for index, (name, drawings, default) in enumerate(pieces_of(document)):
        box = shared_box(drawings)
        files = art_json.drawing_names(index, name, len(drawings))
        for node, file in zip(drawings, files):
            if not save_png(node, box, os.path.join(folder, file), document):
                raise OSError("Could not save %s" % file)
        described.append({"name": name, "x": box.x(), "y": box.y(),
                          "w": box.width(), "h": box.height(),
                          "drawings": files, "default": default})
    art = art_json.build(document.width(), document.height(), described)
    with open(os.path.join(folder, "art.json"), "w") as file:
        json.dump(art, file, indent=2)
    return len(described)


def export(document, parent):
    reason = problem(document)
    if reason:
        QMessageBox.warning(parent, TITLE, reason)
        return
    path = document.fileName()
    name = os.path.splitext(os.path.basename(path))[0] if path else "Doll"
    target = os.path.join(library_folder(), art_json.folder_name(name))
    # Built beside the old doll and swapped in, so a failed export
    # leaves the old one whole. The rig is carried over untouched.
    staging = target + ".part"
    shutil.rmtree(staging, ignore_errors=True)
    try:
        count = write_doll(document, staging)
        old_rig = os.path.join(target, RIG_FILE)
        if os.path.exists(old_rig):
            shutil.copy2(old_rig, os.path.join(staging, RIG_FILE))
        shutil.rmtree(target, ignore_errors=True)
        os.replace(staging, target)
    except OSError as error:
        shutil.rmtree(staging, ignore_errors=True)
        QMessageBox.warning(parent, TITLE, "Export failed: %s" % error)
        return
    QMessageBox.information(
        parent, TITLE, "Exported \"%s\" to Snapper with %d pieces. Its "
        "rig is kept; Snapper picks up the new art when it opens it."
        % (name, count))


class SnapperDollExport(Extension):
    def setup(self):
        pass

    def createActions(self, window):
        action = window.createAction("snapper_doll_export", TITLE + "...",
                                     "tools/scripts")
        action.triggered.connect(self.run)

    def run(self):
        # Asked for at click time: Krita frees the Window handed to
        # createActions once the menus are built.
        krita = Krita.instance()
        export(krita.activeDocument(), krita.activeWindow().qwindow())
