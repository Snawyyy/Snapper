#include "ui/wave_screen.h"

#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "media/audio_clip.h"
#include "ui/audio_preview.h"
#include "ui/bevel.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The phosphor: dark green glass, a dim grid, a bright trace.
constexpr QColor kGlass{0x08, 0x16, 0x0e};
constexpr QColor kGrid{0x14, 0x34, 0x20};
constexpr QColor kTrace{0x5c, 0xff, 0x96};
constexpr QColor kTraceDim{0x2a, 0x8c, 0x52};
constexpr int kColumns = 320;
constexpr int kGridCells = 8;
constexpr int kTickMs = 33;
constexpr int kInset = 2;
constexpr double kMinHeightEms = 3.0;

}  // namespace

WaveScreen::WaveScreen(AudioPreview* preview, const QString& idle,
                       QWidget* parent)
    : QWidget(parent), preview_(preview), idle_(idle) {
  assert(preview != nullptr);
  assert(!idle.isEmpty());
  setMinimumHeight(static_cast<int>(theme::Em(this) * kMinHeightEms));
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setCursor(Qt::PointingHandCursor);
  tick_.setInterval(kTickMs);
  connect(&tick_, &QTimer::timeout, this, [this] { update(); });
  connect(preview_, &AudioPreview::Changed, this,
          &WaveScreen::FollowPreview);
}

void WaveScreen::SetFile(const QString& path, const QString& name) {
  assert(kColumns > 0);
  assert(path.size() < 100000);
  const bool is_same = path == path_ && name == name_;
  if (is_same) {
    return;
  }
  path_ = path;
  name_ = name;
  peaks_.clear();
  seconds_ = 0.0;
  const auto clip = path.isEmpty() ? Result<AudioClip>(AudioClip{})
                                   : DecodeAudio(path);
  // A file that won't decode shows as silence; the panel says why.
  const bool is_decoded = clip.has_value();
  if (is_decoded) {
    peaks_ = Peaks(*clip, kColumns);
    seconds_ = clip->Seconds();
    // Scaled to its loudest, so a quiet line still fills the screen.
    const auto loudest = std::max_element(peaks_.begin(), peaks_.end());
    const bool can_scale = loudest != peaks_.end() && *loudest > 0.0f;
    if (can_scale) {
      const float gain = 1.0f / *loudest;
      for (float& peak : peaks_) {
        peak *= gain;
      }
    }
  }
  setToolTip(path_.isEmpty() ? QString()
                             : tr("Click to listen from that point."));
  FollowPreview();
  update();
}

void WaveScreen::FollowPreview() {
  assert(preview_ != nullptr);
  assert(tick_.interval() == kTickMs);
  const bool is_mine = preview_->IsPlaying(path_);
  if (is_mine) {
    tick_.start();
  } else {
    tick_.stop();
  }
  update();
}

void WaveScreen::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(preview_ != nullptr);
  const bool has_sound = !path_.isEmpty() && seconds_ > 0.0 && width() > 0;
  if (!has_sound) {
    QWidget::mousePressEvent(event);
    return;
  }
  const double share =
      std::clamp(event->position().x() / width(), 0.0, 1.0);
  preview_->PlayFrom(path_, share * seconds_);
}

void WaveScreen::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(preview_ != nullptr);
  QPainter painter(this);
  const QRectF box = QRectF(rect()).adjusted(0, 0, -1, -1);
  painter.fillRect(box, kGlass);
  const QRectF glass = box.adjusted(kInset, kInset, -kInset, -kInset);
  PaintGrid(&painter, glass);
  PaintWave(&painter, glass);
  PaintLabels(&painter, glass);
  DrawBevel(&painter, box, true, kInset);
}

void WaveScreen::PaintGrid(QPainter* painter, const QRectF& glass) const {
  assert(painter != nullptr);
  assert(kGridCells > 0);
  painter->setPen(kGrid);
  for (int cell = 1; cell < kGridCells; ++cell) {
    const double x = glass.left() + glass.width() * cell / kGridCells;
    painter->drawLine(QPointF(x, glass.top()), QPointF(x, glass.bottom()));
  }
  const double middle = glass.center().y();
  for (const double y : {glass.top() + glass.height() / 4.0, middle,
                         glass.bottom() - glass.height() / 4.0}) {
    painter->drawLine(QPointF(glass.left(), y), QPointF(glass.right(), y));
  }
}

void WaveScreen::PaintWave(QPainter* painter, const QRectF& glass) const {
  assert(painter != nullptr);
  assert(peaks_.size() <= static_cast<size_t>(kColumns));
  const bool is_playing = preview_->IsPlaying(path_) && seconds_ > 0.0;
  const double played =
      is_playing ? std::clamp(preview_->Seconds() / seconds_, 0.0, 1.0)
                 : 0.0;
  const int count = static_cast<int>(peaks_.size());
  const double middle = glass.center().y();
  const double reach = glass.height() / 2.0 - 1.0;
  for (int column = 0; column < count; ++column) {
    const double x = glass.left() + glass.width() * (column + 0.5) / count;
    const double half = std::max(
        0.5, reach * peaks_[static_cast<size_t>(column)]);
    const bool is_played = is_playing && column < played * count;
    painter->setPen(is_played ? kTraceDim : kTrace);
    painter->drawLine(QPointF(x, middle - half), QPointF(x, middle + half));
  }
  if (is_playing) {
    const double x = glass.left() + glass.width() * played;
    painter->setPen(QPen(theme::kPick, 2.0));
    painter->drawLine(QPointF(x, glass.top()), QPointF(x, glass.bottom()));
  }
}

void WaveScreen::PaintLabels(QPainter* painter, const QRectF& glass) const {
  assert(painter != nullptr);
  assert(!idle_.isEmpty());
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono.setBold(true);
  painter->setFont(mono);
  painter->setPen(kTrace);
  const bool is_empty = path_.isEmpty();
  if (is_empty) {
    // The words sit on a patch of bare glass, clear of the grid.
    const QRectF words = painter->boundingRect(glass, Qt::AlignCenter, idle_)
                             .adjusted(-6, -1, 6, 1);
    painter->fillRect(words, kGlass);
    painter->drawText(glass, Qt::AlignCenter, idle_);
    return;
  }
  const QRectF text = glass.adjusted(4, 2, -4, -2);
  const int whole = static_cast<int>(seconds_);
  const int hundredths = static_cast<int>((seconds_ - whole) * 100.0);
  painter->drawText(text, Qt::AlignRight | Qt::AlignTop,
                    QStringLiteral("%1.%2 s")
                        .arg(whole)
                        .arg(hundredths, 2, 10, QLatin1Char('0')));
  painter->drawText(text, Qt::AlignLeft | Qt::AlignTop, name_.toUpper());
}

}  // namespace snapper
