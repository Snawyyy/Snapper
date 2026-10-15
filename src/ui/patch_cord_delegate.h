#ifndef SNAPPER_UI_PATCH_CORD_DELEGATE_H_
#define SNAPPER_UI_PATCH_CORD_DELEGATE_H_

#include <QModelIndex>
#include <QSize>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

class QPainter;

namespace snapper {

// What the Rig panel's IK chain list keeps on each row: the two pieces
// a chain joins and which way its middle joint bends.
inline constexpr int kChainUpperRole = Qt::UserRole + 20;
inline constexpr int kChainLowerRole = Qt::UserRole + 21;
inline constexpr int kChainClockwiseRole = Qt::UserRole + 22;

// Draws an IK chain as a patch cord: a jack at each end named for the
// piece it plugs into (upper left, lower right), a red cord sagging
// between them, and on the cord an arrow turning the way the joint
// bends. The picked chain's cord is lit.
class PatchCordDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

  static int RowHeight();
};

}  // namespace snapper

#endif  // SNAPPER_UI_PATCH_CORD_DELEGATE_H_
