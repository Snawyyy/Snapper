#ifndef SNAPPER_UI_COMP_DRAG_H_
#define SNAPPER_UI_COMP_DRAG_H_

#include <QColor>
#include <QIcon>
#include <QMimeData>

#include <memory>
#include <optional>

#include "edit/comp_manager.h"
#include "model/comp.h"

namespace snapper {

// What the Comp tab drags: a library entry (onto the effect list or a
// doll on the stage) or a listed effect (to reorder it).
std::unique_ptr<QMimeData> EntryMime(const CompEntry& entry);
std::optional<CompEntry> EntryOf(const QMimeData* mime);
std::unique_ptr<QMimeData> EffectMime(CompEffectId id);
std::optional<CompEffectId> EffectOf(const QMimeData* mime);

// The color mark of a kind, the same in the library and on the plates.
QColor KindMark(CompKind kind);
// A small square of the mark, for list entries.
QIcon MarkIcon(CompKind kind);

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_DRAG_H_
