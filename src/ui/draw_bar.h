#ifndef SNAPPER_UI_DRAW_BAR_H_
#define SNAPPER_UI_DRAW_BAR_H_

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <array>

#include "edit/draw_manager.h"
#include "ui/managers.h"

namespace snapper {

// Draw mode's title bar: "Drawing: mouth 2 (mouth)", the name the
// drawing is saved as, the onion switch, the brush size, and Done and
// Cancel. Cancel with something drawn asks in place (Discard or Keep
// drawing), never in a window.
class DrawBar final : public QWidget {
  Q_OBJECT

 public:
  explicit DrawBar(const Managers& managers);

  // Done: saves under the name typed, or toasts why not.
  void Finish();
  // Cancel: leaves at once when nothing was drawn, else asks.
  void AskCancel();

 private:
  void Refresh();
  void Ask(bool is_asking);

  Managers managers_;
  QHBoxLayout layout_;
  QLabel title_;
  QLineEdit name_;
  QCheckBox onion_;
  // Done also keeps a mirrored copy (DrawManager::mirrors).
  QCheckBox mirror_;
  QLabel size_;
  // The brush size: typed, wheeled or stepped with [ and ].
  QSpinBox brush_;
  QLabel question_;
  QPushButton discard_;
  QPushButton keep_;
  QPushButton done_;
  QPushButton cancel_;
};

// Draw mode's tool tiles, beside the stage, each with its key on it.
class DrawTools final : public QWidget {
  Q_OBJECT

 public:
  explicit DrawTools(const Managers& managers);

 private:
  void Refresh();

  Managers managers_;
  QVBoxLayout layout_;
  std::array<QToolButton, kDrawToolCount> tiles_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAW_BAR_H_
