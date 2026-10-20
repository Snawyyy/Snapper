#ifndef SNAPPER_UI_CEL_STACK_H_
#define SNAPPER_UI_CEL_STACK_H_

#include <QListWidget>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QStyledItemDelegate>
#include <QWidget>

#include "model/layer.h"

namespace snapper {

// What each cel row keeps: its layer id, whether it shows, whether it
// is locked, and its content's kind (the LayerContent index).
inline constexpr int kCelIdRole = Qt::UserRole;
inline constexpr int kCelShownRole = Qt::UserRole + 1;
inline constexpr int kCelLockedRole = Qt::UserRole + 2;
inline constexpr int kCelKindRole = Qt::UserRole + 3;

// The eye and the lock on a cel drawn in row.
QRect CelEyeRect(const QRect& row);
QRect CelLockRect(const QRect& row);

// Where a layer lands when the cel at from_row (rows top first) is
// dropped in slot (0 above the top row, count under the bottom one), as
// a layer index (0 is the bottom); -1 when it stays where it was.
int CelDropIndex(int from_row, int slot, int count);

// Paints a list row as an animation cel: a clear sheet with a peg hole,
// a chip of its kind, its name, an eye and a lock. A hidden cel slides
// aside and dims; the picked one wears a red outline.
class CelDelegate final : public QStyledItemDelegate {
  Q_OBJECT

 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
};

// Makes a list a stack of cels: installs the delegate, turns clicks on
// an eye or a lock into ShownClicked and LockClicked, and lets a cel be
// dragged up or down: it lifts and follows the pointer, a yellow slot
// shows where it lands, Escape puts it back. It decides nothing; what
// the clicks and drops do is the panel's.
class CelStack final : public QObject {
  Q_OBJECT

 public:
  // Install after PickList, so the eye and lock are seen first.
  static CelStack* Install(QListWidget* list);
  explicit CelStack(QListWidget* list);

 signals:
  void ShownClicked(LayerId layer);
  void LockClicked(LayerId layer);
  // Dropped to layer index index (0 is the bottom).
  void Moved(LayerId layer, int index);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  bool Press(const QPoint& at);
  bool Drag(const QPoint& at);
  bool Drop(const QPoint& at);
  void Cancel();
  // The slot a pointer at y would drop into.
  int SlotAt(int y) const;
  void PaintOverlay();

  QListWidget* list_;
  QWidget* overlay_;
  int from_row_ = -1;
  QPoint press_at_;
  QPoint pointer_;
  bool is_dragging_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CEL_STACK_H_
