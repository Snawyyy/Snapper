from krita import Krita

from .exporter import SnapperDollExport

Krita.instance().addExtension(SnapperDollExport(Krita.instance()))
