#ifndef SNAPPER_UI_EFFECT_ROW_DELEGATE_H_
#define SNAPPER_UI_EFFECT_ROW_DELEGATE_H_

#include <QModelIndex>
#include <QSize>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

class QPainter;

namespace snapper {

// What the Comp effect list keeps on each row.
inline constexpr int kEffectKindRole = Qt::UserRole;
inline constexpr int kEffectPluginRole = Qt::UserRole + 1;
inline constexpr int kEffectSceneRole = Qt::UserRole + 2;
inline constexpr int kEffectHeaderRole = Qt::UserRole + 3;
inline constexpr int kEffectHelpRole = Qt::UserRole + 4;
// On a group's header: true while the group is folded shut, and how many
// effects it holds.
inline constexpr int kEffectFoldedRole = Qt::UserRole + 5;
inline constexpr int kEffectCountRole = Qt::UserRole + 6;

// Draws the Comp effect list. A group's header is a fold bar: raised,
// an arrow, its name in small capitals and its count at the right. An
// effect is a row with a band of its kind's color down its left, lit
// under the pointer and outlined red when picked; one that cannot be
// added is quiet.
class EffectRowDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;

 private:
  static void PaintHeader(QPainter* painter,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index);
  static void PaintEffect(QPainter* painter,
                          const QStyleOptionViewItem& option,
                          const QModelIndex& index);
};

}  // namespace snapper

#endif  // SNAPPER_UI_EFFECT_ROW_DELEGATE_H_
