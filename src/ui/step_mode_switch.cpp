#include "ui/step_mode_switch.h"

#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>

#include <algorithm>
#include <cassert>

#include "ui/theme.h"
#include "ui/timeline_layout.h"

namespace snapper {
namespace {

constexpr int kIconSize = 14;

// A film frame: a box with a sprocket hole at each corner of its sides.
QPixmap FilmFrame(const QColor& color) {
  assert(kIconSize > 4);
  assert(color.isValid());
  QPixmap pixmap(kIconSize, kIconSize);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setPen(QPen(color, 1.0));
  painter.drawRect(QRectF(1.5, 2.5, kIconSize - 3.0, kIconSize - 5.0));
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  for (const double y : {3.5, 8.5}) {
    painter.drawRect(QRectF(2.5, y, 1.5, 1.5));
    painter.drawRect(QRectF(kIconSize - 4.0, y, 1.5, 1.5));
  }
  return pixmap;
}

// A key: a filled diamond.
QPixmap Diamond(const QColor& color) {
  assert(kIconSize > 4);
  assert(color.isValid());
  QPixmap pixmap(kIconSize, kIconSize);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(color);
  const double middle = kIconSize / 2.0;
  painter.drawPolygon(QPolygonF({QPointF(middle, 1.5),
                                 QPointF(kIconSize - 1.5, middle),
                                 QPointF(middle, kIconSize - 1.5),
                                 QPointF(1.5, middle)}));
  return pixmap;
}

// An icon that shows plain, and red once it is the one in use.
QIcon IconOf(QPixmap (*draw)(const QColor&)) {
  assert(draw != nullptr);
  QIcon icon;
  icon.addPixmap(draw(theme::kText), QIcon::Normal, QIcon::Off);
  icon.addPixmap(draw(theme::kPick.lighter(130)), QIcon::Normal, QIcon::On);
  assert(!icon.isNull());
  return icon;
}

}  // namespace

StepModeSwitch::StepModeSwitch(PlaybackManager* playback, QWidget* parent)
    : QWidget(parent), playback_(playback), layout_(this) {
  assert(playback_ != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(1);
  scrub_.setObjectName("step_scrub");
  animation_.setObjectName("step_animation");
  scrub_.setIcon(IconOf(FilmFrame));
  animation_.setIcon(IconOf(Diamond));
  for (QToolButton* button : {&scrub_, &animation_}) {
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setFocusPolicy(Qt::NoFocus);
    button->setIconSize(QSize(kIconSize, kIconSize));
    group_.addButton(button);
    layout_.addWidget(button, 1);
  }
  scrub_.setToolTip(tr("Scrub: the arrow keys move one frame at a time."));
  animation_.setToolTip(tr("Animation: the arrow keys jump to the next frame "
                           "where something changes, skipping held "
                           "frames."));
  connect(&scrub_, &QToolButton::clicked, this, [this] {
    playback_->SetStepMode(StepMode::kScrub);
  });
  connect(&animation_, &QToolButton::clicked, this, [this] {
    playback_->SetStepMode(StepMode::kAnimation);
  });
  connect(playback_, &PlaybackManager::StepModeChanged, this,
          &StepModeSwitch::Show);
  Show();
  assert(group_.buttons().size() == 2);
}

QRect StepModeSwitch::CornerRect(int names_width) {
  assert(names_width > 2);
  assert(kRulerHeight > 2);
  return QRect(1, 1, std::min(names_width, kNameWidth) - 2,
               kRulerHeight - 2);
}

void StepModeSwitch::Show() {
  assert(playback_ != nullptr);
  const bool is_animation = playback_->step_mode() == StepMode::kAnimation;
  scrub_.setChecked(!is_animation);
  animation_.setChecked(is_animation);
  assert(scrub_.isChecked() != animation_.isChecked());
}

}  // namespace snapper
