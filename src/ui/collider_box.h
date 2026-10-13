#ifndef SNAPPER_UI_COLLIDER_BOX_H_
#define SNAPPER_UI_COLLIDER_BOX_H_

#include <QCheckBox>
#include <QLabel>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <vector>

#include "ui/managers.h"

namespace snapper {

// The rig panel's list of what a collider pushes: every other piece of
// the doll, ticked while the collider pushes its warp
// dots (greyed, with the reason, for a piece with no warp grid). The
// list scrolls past a few rows, so a big doll never pushes the panel off
// the screen. Shown only while the focused piece is a collider, as a
// plated section of its own (plated_box.h).
class ColliderBox final : public QWidget {
  Q_OBJECT

 public:
  explicit ColliderBox(const Managers& managers);

  // Shows the collider of the drawing piece shows in the Rig tab (the
  // rig's default, or skin's while skin is being changed; empty: none),
  // or hides the box when it has none.
  void Show(const QString& doll, const QString& piece, const QString& skin);

 signals:
  void Problem(const QString& why);

 private:
  Managers managers_;
  QString doll_;
  QString piece_;
  // The name of the drawing whose collider shows.
  QString drawing_;
  QVBoxLayout layout_;
  QLabel note_;
  QScrollArea scroll_;
  // The ticks' column, inside scroll_ (which owns it).
  QVBoxLayout* ticks_layout_ = nullptr;
  std::vector<std::unique_ptr<QCheckBox>> ticks_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COLLIDER_BOX_H_
