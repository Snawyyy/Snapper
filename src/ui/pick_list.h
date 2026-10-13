#ifndef SNAPPER_UI_PICK_LIST_H_
#define SNAPPER_UI_PICK_LIST_H_

#include <QAbstractItemView>
#include <QEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPersistentModelIndex>
#include <QPoint>

#include "edit/selection_manager.h"

namespace snapper {

// Makes a Qt item view (list, table, tree, icon grid) pick by the one
// pick rule (pick_keys.h) instead of Qt's own: a click picks one, Shift
// adds, Ctrl flips (and wins over Shift), a plain press on a picked item
// keeps the pick so all of it can be dragged (a release without a drag
// then picks only that item), a click on empty space clears. Ctrl+A
// picks every row, Escape clears, Delete and Backspace ask to delete
// the pick (DeleteAsked; the panel calls its manager). Arrow keys move
// the pick, Shift+arrow adds to it. A right click on an unpicked item
// picks it, so its menu acts on what it shows.
//
// The view's own selection mode becomes NoSelection: the selection
// model is still the pick, but only this filter writes it, so dragging
// picked items out still works. Install after the view has its model.
class PickList final : public QObject {
  Q_OBJECT

 public:
  // The view owns the filter.
  static PickList* Install(QAbstractItemView* view);

 signals:
  // Delete or Backspace with something picked.
  void DeleteAsked();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  explicit PickList(QAbstractItemView* view);

  void Press(const QMouseEvent* event);
  void Release(const QMouseEvent* event);
  bool Key(const QKeyEvent* event);
  bool IsOwnKey(const QKeyEvent* event) const;
  void Apply(const QModelIndex& index, PickMode mode);
  void PickAll();
  void FollowCurrent(const QModelIndex& current);
  QItemSelectionModel::SelectionFlags Rows() const;

  QAbstractItemView* view_;
  // A plain press on a picked item: narrowed to it on a release that
  // did not drag.
  QPersistentModelIndex kept_;
  QPoint pressed_at_;
  // An arrow key is moving the current item: it picks what it reaches.
  bool is_keying_ = false;
  bool is_keying_shift_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_PICK_LIST_H_
