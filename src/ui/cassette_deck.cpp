#include "ui/cassette_deck.h"

#include <QFont>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "base/frame.h"
#include "edit/playback_manager.h"

namespace snapper {
namespace {

constexpr double kAspect = 0.62;
constexpr int kMinWidth = 160;
constexpr int kHintWidth = 240;
// A reel turns this far each frame: one turn a second at 24 frames.
constexpr double kDegreesPerFrame = 360.0 / 24.0;
// Pixels of drag per frame scrubbed.
constexpr double kDragPerFrame = 3.0;
constexpr int kHubTeeth = 6;
constexpr QColor kCream{0xec, 0xe6, 0xd8};
constexpr QColor kCreamShade{0xc9, 0xc1, 0xae};
constexpr QColor kStripe{0xc4, 0x26, 0x2c};
constexpr QColor kPlate{0xd6, 0xd6, 0xd8};
constexpr QColor kWindowRim{0x16, 0x16, 0x18};
constexpr QColor kShell{0x7c, 0x7c, 0x82};
constexpr QColor kTape{0x2a, 0x1f, 0x1a};
constexpr QColor kHub{0xe2, 0xe2, 0xe4};

}  // namespace

CassetteDeck::CassetteDeck(const Managers& managers) : managers_(managers) {
  assert(managers_.playback != nullptr);
  assert(kAspect > 0.0);
  PlaybackManager* playback = managers_.playback;
  setMinimumWidth(kMinWidth);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  setCursor(Qt::OpenHandCursor);
  setFocusPolicy(Qt::ClickFocus);
  setToolTip(tr("The tape follows the playhead. Drag across it to "
                "scrub; Escape while dragging puts it back."));
  connect(playback, &PlaybackManager::FrameChanged, this,
          [this] { update(); });
  connect(playback, &PlaybackManager::TimelineChanged, this,
          [this] { update(); });
}

int CassetteDeck::heightForWidth(int width) const {
  assert(width >= 0);
  assert(kAspect > 0.0);
  return static_cast<int>(width * kAspect);
}

QSize CassetteDeck::sizeHint() const {
  assert(kMinWidth > 0);
  assert(kHintWidth >= kMinWidth);
  return {kHintWidth, heightForWidth(kHintWidth)};
}

double CassetteDeck::Wound() const {
  assert(managers_.playback != nullptr);
  const int last = managers_.playback->LastFrame().index();
  assert(last >= 0);
  const bool is_empty = last == 0;
  if (is_empty) {
    return 0.0;
  }
  const int frame = managers_.playback->frame().index();
  return std::clamp(static_cast<double>(frame) / last, 0.0, 1.0);
}

void CassetteDeck::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(width() > 0);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  // Keep the walkman's shape whatever room the dock gives it.
  const double room_w = width() - 8.0;
  const double room_h = height() - 8.0;
  const double body_w = std::min(room_w, room_h / kAspect);
  const double body_h = body_w * kAspect;
  const QRectF body((width() - body_w) / 2.0, (height() - body_h) / 2.0,
                    body_w, body_h);
  PaintBody(&painter, body);
  const QRectF window(body.left() + body_w * 0.07,
                      body.top() + body_h * 0.36, body_w * 0.86,
                      body_h * 0.54);
  PaintWindow(&painter, window);
}

void CassetteDeck::PaintBody(QPainter* painter, const QRectF& body) const {
  assert(painter != nullptr);
  assert(body.width() > 0.0);
  const double corner = body.height() * 0.08;
  QLinearGradient shade(body.topLeft(), body.bottomRight());
  shade.setColorAt(0.0, kCream);
  shade.setColorAt(1.0, kCreamShade);
  painter->setPen(QPen(kCreamShade.darker(150), 1.0));
  painter->setBrush(shade);
  painter->drawRoundedRect(body, corner, corner);
  // The red line along the top, as on the old walkman.
  const double stripe_y = body.top() + body.height() * 0.06;
  painter->setPen(QPen(kStripe, std::max(1.5, body.height() * 0.018)));
  painter->drawLine(QPointF(body.left() + corner, stripe_y),
                    QPointF(body.right() - corner, stripe_y));
  // The silver plate with the name, the word in red beside it.
  const QRectF plate(body.left() + body.width() * 0.07,
                     body.top() + body.height() * 0.11,
                     body.width() * 0.36, body.height() * 0.18);
  painter->setPen(QPen(kPlate.darker(130), 1.0));
  painter->setBrush(kPlate);
  painter->drawRect(plate);
  QFont serif(QStringLiteral("serif"));
  serif.setPixelSize(std::max(6, static_cast<int>(plate.height() * 0.55)));
  serif.setLetterSpacing(QFont::PercentageSpacing, 130);
  painter->setFont(serif);
  painter->setPen(QColor(0x1a, 0x1a, 0x1a));
  painter->drawText(plate, Qt::AlignCenter, QStringLiteral("STEREO"));
  QFont heavy;
  heavy.setPixelSize(std::max(6, static_cast<int>(plate.height() * 0.8)));
  heavy.setWeight(QFont::Black);
  heavy.setItalic(true);
  painter->setFont(heavy);
  painter->setPen(kStripe);
  const QRectF word(plate.right() + body.width() * 0.04, plate.top(),
                    body.right() - plate.right() - body.width() * 0.1,
                    plate.height());
  painter->drawText(word, Qt::AlignRight | Qt::AlignVCenter,
                    QStringLiteral("SNAPMAN"));
}

void CassetteDeck::PaintWindow(QPainter* painter,
                               const QRectF& window) const {
  assert(painter != nullptr);
  assert(managers_.playback != nullptr);
  const double round = window.height() / 2.0;
  painter->setPen(Qt::NoPen);
  painter->setBrush(kWindowRim);
  painter->drawRoundedRect(window, round, round);
  const QRectF shell = window.adjusted(round * 0.35, round * 0.18,
                                       -round * 0.35, -round * 0.18);
  QLinearGradient grey(shell.topLeft(), shell.bottomLeft());
  grey.setColorAt(0.0, kShell.lighter(115));
  grey.setColorAt(1.0, kShell.darker(115));
  painter->setBrush(grey);
  painter->drawRoundedRect(shell, shell.height() * 0.12,
                           shell.height() * 0.12);
  const double hub = shell.height() * 0.17;
  const double full = shell.height() * 0.44;
  const double wound = Wound();
  // A pack's area grows with the tape on it, so its radius goes by the
  // square root.
  // An empty reel still shows a thin ring of leader tape.
  const double core = hub * 1.12;
  const auto pack_of = [core, full](double share) {
    return std::sqrt(core * core + (full * full - core * core) * share);
  };
  const double turn = managers_.playback->frame().index() * kDegreesPerFrame;
  const double mid_y = shell.center().y();
  PaintReel(painter, QPointF(shell.left() + shell.width() * 0.28, mid_y),
            hub, pack_of(1.0 - wound), turn);
  PaintReel(painter, QPointF(shell.left() + shell.width() * 0.72, mid_y),
            hub, pack_of(wound), turn);
  // Glass over it all: a soft light off the top.
  QLinearGradient glare(window.topLeft(), window.bottomLeft());
  glare.setColorAt(0.0, QColor(255, 255, 255, 40));
  glare.setColorAt(0.5, QColor(255, 255, 255, 0));
  painter->setBrush(glare);
  painter->drawRoundedRect(window, round, round);
}

void CassetteDeck::PaintReel(QPainter* painter, const QPointF& middle,
                             double hub, double pack, double turn) const {
  assert(painter != nullptr);
  assert(hub > 0.0 && pack >= hub);
  painter->setPen(Qt::NoPen);
  painter->setBrush(kTape);
  painter->drawEllipse(middle, pack, pack);
  painter->setBrush(kHub);
  painter->drawEllipse(middle, hub, hub);
  painter->save();
  painter->translate(middle);
  painter->rotate(turn);
  // The teeth that grip the spindle: they show the turn.
  painter->setBrush(kWindowRim);
  const double tooth = hub * 0.28;
  for (int i = 0; i < kHubTeeth; ++i) {
    painter->drawRect(QRectF(hub * 0.45, -tooth / 2.0, hub * 0.4, tooth));
    painter->rotate(360.0 / kHubTeeth);
  }
  painter->drawEllipse(QPointF(0.0, 0.0), hub * 0.3, hub * 0.3);
  painter->restore();
}

void CassetteDeck::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.playback != nullptr);
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    QWidget::mousePressEvent(event);
    return;
  }
  is_dragging_ = true;
  drag_x_ = event->position().x();
  drag_frame_ = managers_.playback->frame().index();
  setCursor(Qt::ClosedHandCursor);
}

void CassetteDeck::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(kDragPerFrame > 0.0);
  if (!is_dragging_) {
    return;
  }
  const double moved = event->position().x() - drag_x_;
  const int frame =
      drag_frame_ + static_cast<int>(std::lround(moved / kDragPerFrame));
  managers_.playback->Seek(Frame(std::max(0, frame)));
}

void CassetteDeck::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.playback != nullptr);
  is_dragging_ = false;
  setCursor(Qt::OpenHandCursor);
}

void CassetteDeck::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(managers_.playback != nullptr);
  const bool is_cancel = is_dragging_ && event->key() == Qt::Key_Escape;
  if (!is_cancel) {
    QWidget::keyPressEvent(event);
    return;
  }
  is_dragging_ = false;
  setCursor(Qt::OpenHandCursor);
  managers_.playback->Seek(Frame(drag_frame_));
}

}  // namespace snapper
