#include "ui/disc_burner.h"

#include <QConicalGradient>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kTickMs = 33;
// Degrees the disc turns each tick while burning.
constexpr double kTurnStep = 14.0;
constexpr int kTrayHeight = 150;
constexpr QColor kTray{0x1a, 0x16, 0x17};
constexpr QColor kInk{0x3a, 0x34, 0x2c};

// A disc's rainbow face, turned turn degrees.
QConicalGradient DiscFace(const QPointF& center, double turn) {
  QConicalGradient face(center, turn);
  const QColor stops[] = {{0xcf, 0xd6, 0xe6}, {0xf5, 0xe8, 0xff},
                          {0xbf, 0xe7, 0xe0}, {0xf2, 0xe9, 0xc9},
                          {0xcf, 0xd6, 0xe6}, {0xe8, 0xd1, 0xef}};
  const int count = static_cast<int>(std::size(stops));
  for (int i = 0; i < count; ++i) {
    face.setColorAt(static_cast<double>(i) / count, stops[i]);
  }
  face.setColorAt(1.0, stops[0]);
  return face;
}

}  // namespace

DiscBurner::DiscBurner(QWidget* parent) : QWidget(parent) {
  spin_.setInterval(kTickMs);
  connect(&spin_, &QTimer::timeout, this, [this] {
    turn_ = turn_ + kTurnStep >= 360.0 ? turn_ + kTurnStep - 360.0
                                       : turn_ + kTurnStep;
    update();
  });
  setMinimumHeight(kTrayHeight);
}

QSize DiscBurner::sizeHint() const {
  return {260, kTrayHeight};
}

void DiscBurner::SetProgress(int done, int all) {
  assert(done >= 0 && all >= 0);
  done_ = done;
  all_ = all;
  label_.clear();
  state_ = State::kBurning;
  if (!spin_.isActive()) {
    spin_.start();
  }
  update();
}

void DiscBurner::Finish(const QString& label) {
  spin_.stop();
  state_ = State::kDone;
  done_ = all_;
  label_ = label;
  update();
}

void DiscBurner::Stop(bool has_failed) {
  spin_.stop();
  state_ = has_failed ? State::kFailed : State::kEmpty;
  done_ = 0;
  label_.clear();
  update();
}

void DiscBurner::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  // The tray: a dark slot with a light lip on top.
  const QRectF tray = QRectF(rect()).adjusted(4.0, 4.0, -4.0, -4.0);
  painter.fillRect(tray, kTray);
  painter.fillRect(QRectF(tray.left(), tray.top(), tray.width(), 1.0),
                   theme::kFaceLight);
  painter.fillRect(QRectF(tray.left(), tray.bottom() - 1.0, tray.width(),
                          2.0),
                   theme::kShadow);
  const double radius = std::min(tray.height() - 34.0, tray.width()) / 2.0;
  const QPointF center(tray.center().x(), tray.top() + 8.0 + radius);
  const bool has_disc = state_ != State::kEmpty;
  if (has_disc) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(DiscFace(center, turn_));
    painter.drawEllipse(center, radius, radius);
    // A failed burn leaves a dull, scratched disc.
    if (state_ == State::kFailed) {
      painter.setBrush(QColor(0, 0, 0, 120));
      painter.drawEllipse(center, radius, radius);
    }
    painter.setBrush(QColor(255, 255, 255, 90));
    painter.drawEllipse(center, radius * 0.42, radius * 0.42);
    painter.setBrush(kTray);
    painter.drawEllipse(center, radius * 0.18, radius * 0.18);
  } else {
    painter.setPen(QPen(theme::kFaceLight, 1.0, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);
    painter.setPen(theme::kTextOff);
    painter.drawText(QRectF(center - QPointF(radius, radius),
                            QSizeF(2 * radius, 2 * radius)),
                     Qt::AlignCenter, tr("Tray empty"));
  }
  // The label written on in marker once it is burned.
  const bool has_label = state_ == State::kDone && !label_.isEmpty();
  if (has_label) {
    QFont marker = font();
    marker.setItalic(true);
    marker.setBold(true);
    painter.setFont(marker);
    painter.setPen(kInk);
    painter.save();
    painter.translate(center + QPointF(0.0, -radius * 0.6));
    painter.rotate(-10.0);
    painter.drawText(QRectF(-radius, -10.0, 2 * radius, 20.0),
                     Qt::AlignCenter,
                     painter.fontMetrics().elidedText(
                         label_, Qt::ElideMiddle,
                         static_cast<int>(radius * 1.6)));
    painter.restore();
  }
  // The burn bar and its counter, under the disc.
  const QRectF bar(tray.left() + 10.0, tray.bottom() - 16.0,
                   tray.width() * 0.5, 6.0);
  painter.fillRect(bar, Qt::black);
  const double share = all_ > 0 ? std::clamp(1.0 * done_ / all_, 0.0, 1.0)
                                : 0.0;
  painter.fillRect(QRectF(bar.topLeft(), QSizeF(bar.width() * share,
                                                bar.height())),
                   theme::kPick);
  QFont mono = font();
  mono.setFamily(QStringLiteral("monospace"));
  painter.setFont(mono);
  painter.setPen(state_ == State::kFailed ? theme::kTextOff : theme::kPick);
  const QString counter =
      state_ == State::kBurning ? tr("BURN %1%").arg(static_cast<int>(share * 100))
      : state_ == State::kDone  ? tr("DONE")
      : state_ == State::kFailed ? tr("FAILED")
                                 : tr("READY");
  painter.drawText(QRectF(bar.right() + 8.0, bar.top() - 6.0,
                          tray.right() - bar.right() - 18.0, 18.0),
                   Qt::AlignRight | Qt::AlignVCenter, counter);
}

}  // namespace snapper
