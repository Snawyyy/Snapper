#ifndef SNAPPER_UI_RIG_TREE_DELEGATE_H_
#define SNAPPER_UI_RIG_TREE_DELEGATE_H_

#include <QModelIndex>
#include <QSize>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

class QPainter;

namespace snapper {

// What the Rig panel's piece list keeps on each row: how deep the piece
// hangs, and a bit per level above it, set where that level's branch
// line runs on past this row (the ancestor has a later child).
inline constexpr int kRigDepthRole = Qt::UserRole + 10;
inline constexpr int kRigLinesRole = Qt::UserRole + 11;
// True on the last child of its parent: its branch ends in an elbow.
inline constexpr int kRigLastRole = Qt::UserRole + 12;
// The deepest a row is drawn indented; deeper pieces line up there.
inline constexpr int kRigMaxDepth = 30;

// Draws the piece list as the doll's tree: branch lines from each piece
// to the one it hangs from, a yellow joint dot (as on the canvas), and
// the name. Picked rows wear Teto red; the focused one is outlined.
class RigTreeDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

  // The height of one row, for sizing the list to its pieces.
  static int RowHeight();
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_TREE_DELEGATE_H_
