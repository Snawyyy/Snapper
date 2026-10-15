#include "ui/rig_tree_delegate.h"

#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cassert>

#include "ui/snapper_accents.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kRowHeight = 20;
// Indent per level, and where the first level's line runs.
constexpr int kStep = 14;
constexpr int kLeft = 8;
constexpr double kDot = 3.0;
constexpr QColor kBranch{0x6a, 0x5e, 0x5a};

}  // namespace

int RigTreeDelegate::RowHeight() {
  assert(kRowHeight > 0);
  assert(kStep > 0);
  return kRowHeight;
}

void RigTreeDelegate::paint(QPainter* painter,
                            const QStyleOptionViewItem& option,
                            const QModelIndex& index) const {
  assert(painter != nullptr);
  assert(index.isValid());
  painter->save();
  const QRect row = option.rect;
  const bool is_picked = option.state.testFlag(QStyle::State_Selected);
  const bool is_hovered = option.state.testFlag(QStyle::State_MouseOver);
  const bool is_focus = option.state.testFlag(QStyle::State_HasFocus);
  painter->fillRect(row, is_picked    ? theme::kPick.darker(170)
                         : is_hovered ? theme::kFace
                                      : theme::kWell);
  const int depth =
      std::clamp(index.data(kRigDepthRole).toInt(), 0, kRigMaxDepth);
  const qulonglong lines = index.data(kRigLinesRole).toULongLong();
  const bool is_last = index.data(kRigLastRole).toBool();
  const double middle = row.center().y() + 0.5;
  painter->setPen(QPen(kBranch, 1.0));
  // Lines of the levels above that carry on past this row.
  for (int level = 0; level + 1 < depth; ++level) {
    const bool runs_on = (lines >> level) & 1U;
    if (runs_on) {
      const double x = row.left() + kLeft + level * kStep;
      painter->drawLine(QPointF(x, row.top()), QPointF(x, row.bottom() + 1));
    }
  }
  const double dot_x = row.left() + kLeft + depth * kStep;
  const bool has_parent = depth > 0;
  if (has_parent) {
    // The elbow into this piece from the line of the one it hangs from.
    const double x = row.left() + kLeft + (depth - 1) * kStep;
    painter->drawLine(QPointF(x, row.top()),
                      QPointF(x, is_last ? middle : row.bottom() + 1));
    painter->drawLine(QPointF(x, middle), QPointF(dot_x - kDot, middle));
  }
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(QPen(theme::kShadow, 1.0));
  painter->setBrush(theme::kHandle);
  painter->drawEllipse(QPointF(dot_x, middle), kDot, kDot);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setPen(theme::kText);
  const QRect words = row.adjusted(static_cast<int>(dot_x - row.left()) + 8,
                                   0, -4, 0);
  painter->drawText(words, Qt::AlignLeft | Qt::AlignVCenter,
                    option.fontMetrics.elidedText(
                        index.data(Qt::DisplayRole).toString(),
                        Qt::ElideRight, std::max(0, words.width())));
  if (is_focus && is_picked) {
    DrawFocusRing(painter, row);
  }
  painter->restore();
}

QSize RigTreeDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const {
  assert(index.isValid());
  assert(option.rect.width() >= 0);
  return QSize(0, kRowHeight);
}

}  // namespace snapper
