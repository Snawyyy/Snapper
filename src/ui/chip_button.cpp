#include "ui/chip_button.h"

#include <QFontMetrics>
#include <QResizeEvent>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

// Room the button's bevel and padding take beside the text.
constexpr int kPadPx = 16;
// The narrowest a chip shrinks to, in pixels.
constexpr int kLeastPx = 40;

}  // namespace

ChipButton::ChipButton(QWidget* parent) : QPushButton(parent) {
  assert(parent != nullptr);
  assert(kLeastPx > kPadPx);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ChipButton::SetFullText(const QString& text) {
  assert(text.size() < 10000);
  assert(kPadPx > 0);
  const bool is_same = text == full_;
  if (is_same) {
    return;
  }
  full_ = text;
  Elide();
  updateGeometry();
}

QSize ChipButton::sizeHint() const {
  assert(kPadPx > 0);
  assert(full_.size() < 10000);
  const QSize base = QPushButton::sizeHint();
  const int wide = fontMetrics().horizontalAdvance(full_) + kPadPx;
  return QSize(std::max(wide, kLeastPx), base.height());
}

QSize ChipButton::minimumSizeHint() const {
  assert(kLeastPx > 0);
  assert(kPadPx > 0);
  return QSize(kLeastPx, QPushButton::sizeHint().height());
}

void ChipButton::resizeEvent(QResizeEvent* event) {
  assert(event != nullptr);
  assert(full_.size() < 10000);
  QPushButton::resizeEvent(event);
  Elide();
}

void ChipButton::Elide() {
  assert(kPadPx > 0);
  assert(full_.size() < 10000);
  const QString shown =
      fontMetrics().elidedText(full_, Qt::ElideRight, width() - kPadPx);
  const bool is_new = shown != text();
  if (is_new) {
    setText(shown);
  }
}

}  // namespace snapper
