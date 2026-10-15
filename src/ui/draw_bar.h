#ifndef SNAPPER_UI_DRAW_BAR_H_
#define SNAPPER_UI_DRAW_BAR_H_

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <memory>

#include "edit/draw_manager.h"
#include "ui/brush_well.h"
#include "ui/managers.h"
#include "ui/tape_key.h"

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
  QLabel size_;
  QLabel question_;
  QPushButton discard_;
  QPushButton keep_;
  QPushButton done_;
  QPushButton cancel_;
};

// Draw mode's tools beside the stage, on a strip of deck panel: a tape
// key each with the tool's mark in its dimple and its key letter in
// the corner (the one in use latched down, red), and under them the
// brush well showing the brush's size and color.
class DrawTools final : public QWidget {
  Q_OBJECT

 public:
  explicit DrawTools(const Managers& managers);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  void Refresh();

  Managers managers_;
  QVBoxLayout layout_;
  std::array<std::unique_ptr<TapeKey>, kDrawToolCount> tiles_;
  BrushWell well_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAW_BAR_H_
