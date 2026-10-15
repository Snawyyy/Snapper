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
#include "ui/theme.h"

namespace snapper {
namespace {

// A compact cassette is 100 by 64 mm.
constexpr double kAspect = 0.64;
constexpr int kMinWidth = 160;
constexpr int kHintWidth = 240;
// A reel turns this far each frame: one turn a second at 24 frames.
constexpr double kDegreesPerFrame = 360.0 / 24.0;
// Pixels of drag per frame scrubbed.
constexpr double kDragPerFrame = 3.0;
constexpr int kHubTeeth = 6;
// The wall's shadow over the cassette's top and left edges: how deep
// it reaches (a share of the height) and how dark it starts.
constexpr double kInsetShare = 0.14;
constexpr int kInsetAlpha = 190;
constexpr int kMaxRidges = 400;
// Where the hubs sit, as shares of the shell's width and height.
constexpr double kLeftHubX = 0.31;
constexpr double kRightHubX = 0.69;
constexpr double kHubY = 0.5;
constexpr QColor kShell{0x1e, 0x1e, 0x20};
constexpr QColor kLabel{0xec, 0xe6, 0xd2};
constexpr QColor kInk{0x3a, 0x34, 0x2c};
constexpr QColor kBrown{0x86, 0x48, 0x1e};
constexpr QColor kOrange{0xd2, 0x7c, 0x2c};
constexpr QColor kWindow{0x26, 0x26, 0x28};
constexpr QColor kTape{0x4a, 0x30, 0x22};
constexpr QColor kHub{0xf0, 0xf0, 0xec};

// The part of shell from (x0, y0) to (x1, y1), each a share of its size.
QRectF Part(const QRectF& shell, double x0, double y0, double x1,
            double y1) {
  assert(x1 >= x0);
  assert(y1 >= y0);
  return QRectF(shell.left() + shell.width() * x0,
                shell.top() + shell.height() * y0,
                shell.width() * (x1 - x0), shell.height() * (y1 - y0));
}

QPointF Spot(const QRectF& shell, double x, double y) {
  assert(x >= 0.0 && x <= 1.0);
  assert(y >= 0.0 && y <= 1.0);
  return {shell.left() + shell.width() * x,
          shell.top() + shell.height() * y};
}

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
  // Keep the cassette's shape whatever room the dock gives it.
  const double room_w = width();
  const double room_h = height();
  const double shell_w = std::min(room_w, room_h / kAspect);
  const double shell_h = shell_w * kAspect;
  const QRectF shell((width() - shell_w) / 2.0, (height() - shell_h) / 2.0,
                     shell_w, shell_h);
  PaintShell(&painter, shell);
  PaintLabel(&painter, shell);
  PaintWindow(&painter, shell);
  PaintInset(&painter, shell);
}

void CassetteDeck::PaintInset(QPainter* painter,
                              const QRectF& shell) const {
  assert(painter != nullptr);
  assert(kInsetShare > 0.0);
  // Deep enough to fall across the label, not just the black rim.
  const double depth = shell.height() * kInsetShare;
  // Set into the wall: the light from the top left leaves the wall's
  // shadow over the top and left edges and catches the bottom and right.
  painter->setPen(Qt::NoPen);
  const QColor dark(0, 0, 0, kInsetAlpha);
  const QColor clear(0, 0, 0, 0);
  QLinearGradient top(shell.topLeft(),
                      shell.topLeft() + QPointF(0.0, depth));
  top.setColorAt(0.0, dark);
  top.setColorAt(1.0, clear);
  painter->fillRect(QRectF(shell.left(), shell.top(), shell.width(),
                           depth), top);
  QLinearGradient left(shell.topLeft(),
                       shell.topLeft() + QPointF(depth, 0.0));
  left.setColorAt(0.0, dark);
  left.setColorAt(1.0, clear);
  painter->fillRect(QRectF(shell.left(), shell.top(), depth,
                           shell.height()), left);
  painter->setPen(QPen(theme::kFaceLight, 1.0));
  painter->drawLine(shell.bottomLeft() - QPointF(0.0, 0.5),
                    shell.bottomRight() - QPointF(0.0, 0.5));
  painter->drawLine(shell.topRight() - QPointF(0.5, 0.0),
                    shell.bottomRight() - QPointF(0.5, 0.0));
}

void CassetteDeck::PaintShell(QPainter* painter,
                              const QRectF& shell) const {
  assert(painter != nullptr);
  assert(shell.width() > 0.0);
  const double corner = shell.height() * 0.04;
  painter->setPen(QPen(QColor(0x05, 0x05, 0x06), 1.0));
  painter->setBrush(kShell);
  painter->drawRoundedRect(shell, corner, corner);
  // The fine grip ridges moulded into the plastic.
  painter->setPen(QColor(255, 255, 255, 14));
  for (int ridge = 0; ridge < kMaxRidges; ++ridge) {
    const double y = shell.top() + 2.0 + ridge * 2.0;
    const bool is_past = y >= shell.bottom() - 2.0;
    if (is_past) {
      break;
    }
    painter->drawLine(QPointF(shell.left() + 2.0, y),
                      QPointF(shell.right() - 2.0, y));
  }
  // The raised block over the tape heads, with its holes.
  QPolygonF block;
  block << Spot(shell, 0.22, 0.78) << Spot(shell, 0.78, 0.78)
        << Spot(shell, 0.83, 1.0) << Spot(shell, 0.17, 1.0);
  painter->setPen(QPen(QColor(0x40, 0x40, 0x44), 1.0));
  painter->setBrush(kShell.lighter(115));
  painter->drawPolygon(block);
  const double hole = shell.width() * 0.018;
  painter->setPen(Qt::NoPen);
  painter->setBrush(theme::kFace);
  for (const double x : {0.27, 0.35, 0.65, 0.73}) {
    painter->drawEllipse(Spot(shell, x, 0.9), hole, hole);
  }
  painter->setBrush(QColor(0x0a, 0x0a, 0x0b));
  painter->drawEllipse(Spot(shell, 0.5, 0.84), hole * 0.8, hole * 0.8);
  // A screw in each corner.
  const double screw = shell.width() * 0.016;
  painter->setBrush(QColor(0x50, 0x50, 0x54));
  for (const double x : {0.03, 0.97}) {
    for (const double y : {0.05, 0.95}) {
      painter->drawEllipse(Spot(shell, x, y), screw, screw);
    }
  }
}

void CassetteDeck::PaintLabel(QPainter* painter,
                              const QRectF& shell) const {
  assert(painter != nullptr);
  assert(shell.height() > 0.0);
  const QRectF label = Part(shell, 0.06, 0.07, 0.94, 0.72);
  painter->setPen(Qt::NoPen);
  painter->setBrush(kLabel);
  painter->drawRoundedRect(label, 2.0, 2.0);
  // The side mark: a black box with a white A.
  const QRectF side = Part(shell, 0.085, 0.1, 0.135, 0.18);
  painter->fillRect(side, kShell);
  QFont bold;
  bold.setBold(true);
  bold.setPixelSize(std::max(6, static_cast<int>(side.height() * 0.8)));
  painter->setFont(bold);
  painter->setPen(kLabel);
  painter->drawText(side, Qt::AlignCenter, QStringLiteral("A"));
  // Lines to write the title on.
  painter->setPen(QPen(QColor(0xb4, 0xac, 0x98), 1.0));
  for (const double y : {0.15, 0.22, 0.29}) {
    painter->drawLine(Spot(shell, 0.16, y), Spot(shell, 0.91, y));
  }
  painter->fillRect(Part(shell, 0.06, 0.34, 0.94, 0.365), kBrown);
  painter->fillRect(Part(shell, 0.06, 0.365, 0.94, 0.385), kOrange);
  QFont small;
  small.setPixelSize(std::max(5, static_cast<int>(shell.height() * 0.042)));
  painter->setFont(small);
  painter->setPen(kInk);
  painter->drawText(Part(shell, 0.08, 0.43, 0.25, 0.6),
                    Qt::AlignLeft | Qt::AlignTop,
                    tr("Normal\n24 fps"));
  painter->drawText(Part(shell, 0.77, 0.43, 0.93, 0.6),
                    Qt::AlignLeft | Qt::AlignTop, tr("Snapper\nC-90"));
}

void CassetteDeck::PaintWindow(QPainter* painter,
                               const QRectF& shell) const {
  assert(painter != nullptr);
  assert(managers_.playback != nullptr);
  const QRectF band = Part(shell, 0.24, 0.41, 0.76, 0.63);
  const double round = band.height() * 0.2;
  painter->setPen(Qt::NoPen);
  painter->setBrush(kWindow);
  painter->drawRoundedRect(band, round, round);
  const double hub = shell.width() * 0.055;
  const double full = shell.width() * 0.185;
  // A pack's area grows with the tape on it, so its radius goes by the
  // square root; an empty reel still holds a ring of leader.
  const double core = hub * 1.15;
  const auto pack_of = [core, full](double share) {
    return std::sqrt(core * core + (full * full - core * core) * share);
  };
  const double wound = Wound();
  const QPointF left = Spot(shell, kLeftHubX, kHubY + 0.02);
  const QPointF right = Spot(shell, kRightHubX, kHubY + 0.02);
  // The tape packs show only through the clear middle window.
  const QRectF clear = Part(shell, 0.4, 0.45, 0.6, 0.59);
  painter->save();
  painter->setClipRect(clear);
  painter->fillRect(clear, QColor(0x5a, 0x5a, 0x5e));
  painter->setBrush(kTape);
  painter->drawEllipse(left, pack_of(1.0 - wound), pack_of(1.0 - wound));
  painter->drawEllipse(right, pack_of(wound), pack_of(wound));
  // The length marks printed on the window.
  painter->setPen(QPen(QColor(255, 255, 255, 150), 1.0));
  for (int mark = 0; mark < 4; ++mark) {
    const double x = clear.left() + clear.width() * (0.3 + 0.13 * mark);
    painter->drawLine(QPointF(x, clear.top() + clear.height() * 0.25),
                      QPointF(x, clear.bottom() - clear.height() * 0.25));
  }
  painter->restore();
  const double turn = managers_.playback->frame().index() * kDegreesPerFrame;
  PaintHub(painter, left, hub, turn);
  PaintHub(painter, right, hub, turn);
}

void CassetteDeck::PaintHub(QPainter* painter, const QPointF& middle,
                            double hub, double turn) const {
  assert(painter != nullptr);
  assert(hub > 0.0);
  painter->setPen(Qt::NoPen);
  painter->setBrush(kHub);
  painter->drawEllipse(middle, hub, hub);
  painter->setBrush(kWindow);
  painter->drawEllipse(middle, hub * 0.62, hub * 0.62);
  painter->save();
  painter->translate(middle);
  painter->rotate(turn);
  // The teeth that grip the spindle: they show the turn.
  painter->setBrush(kHub);
  const double tooth = hub * 0.3;
  for (int i = 0; i < kHubTeeth; ++i) {
    painter->drawRect(QRectF(hub * 0.36, -tooth / 2.0, hub * 0.3, tooth));
    painter->rotate(360.0 / kHubTeeth);
  }
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
