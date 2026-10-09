#include "ui/step_button.h"

#include <QStyleOptionButton>
#include <QStylePainter>

#include <cassert>

#include "ui/motion.h"

namespace snapper {
namespace {

// Button text is read for & mnemonics; a step's name may hold one.
QString Plain(const QString& text) {
  assert(text.size() < 4096);
  assert(text.size() >= 0);
  QString plain = text;
  return plain.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

}  // namespace

StepButton::StepButton(const QString& verb, MotionManager* motion)
    : QPushButton(verb), motion_(motion), words_(verb) {
  assert(motion != nullptr);
  assert(!verb.isEmpty());
  setFocusPolicy(Qt::NoFocus);
}

void StepButton::ShowFace(const StepFace& face) {
  assert(!face.text.isEmpty());
  assert(motion_ != nullptr);
  setEnabled(face.enabled);
  // A name too long for the button is still readable in full.
  setToolTip(face.enabled ? face.text : face.tooltip);
  const bool is_new = face.text != words_;
  if (!is_new) {
    return;
  }
  const bool is_running = !animation_.isNull();
  if (is_running) {
    animation_->stop();
  }
  old_words_ = words_;
  words_ = face.text;
  setText(Plain(words_));
  slide_ = 0.0;
  animation_ = motion_->Animate(
      this, MotionKind::kPop, 0.0, 1.0, [this](const QVariant& value) {
        slide_ = value.toDouble();
        update();
      });
}

void StepButton::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(slide_ >= -1.0 && slide_ <= 2.0);
  const bool is_at_rest = slide_ >= 1.0;
  if (is_at_rest) {
    QPushButton::paintEvent(event);
    return;
  }
  QStylePainter painter(this);
  QStyleOptionButton option;
  initStyleOption(&option);
  option.text.clear();
  painter.drawControl(QStyle::CE_PushButton, option);
  const QRect area = style()->subElementRect(QStyle::SE_PushButtonContents,
                                             &option, this);
  painter.setClipRect(area);
  const bool is_on = isEnabled();
  painter.setPen(palette().color(is_on ? QPalette::Active
                                       : QPalette::Disabled,
                                 QPalette::ButtonText));
  const int rise = static_cast<int>(slide_ * area.height());
  painter.drawText(area.translated(0, -rise), Qt::AlignCenter, old_words_);
  painter.drawText(area.translated(0, area.height() - rise),
                   Qt::AlignCenter, words_);
}

}  // namespace snapper
