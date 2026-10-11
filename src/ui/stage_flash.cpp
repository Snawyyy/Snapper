#include "ui/stage_flash.h"

#include <QColor>
#include <QPainter>

#include <algorithm>
#include <cassert>

#include "ui/motion.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Where the flash is faintest, against the new one at full.
constexpr double kWasShare = 0.6;

// A copy of picture with every pixel colour, keeping its see-through.
QImage Tinted(const QImage& picture, QColor colour) {
  assert(colour.isValid());
  assert(picture.format() != QImage::Format_Invalid);
  QImage out = picture.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QPainter painter(&out);
  painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
  painter.fillRect(out.rect(), colour);
  return out;
}

}  // namespace

StageFlash::StageFlash(MotionManager* motion, QObject* parent)
    : QObject(parent), motion_(motion) {
  assert(motion_ != nullptr);
  assert(parent != nullptr);
}

void StageFlash::Show(const QImage& was, const QImage& now,
                      QPointF corner) {
  assert(motion_ != nullptr);
  assert(was.isNull() == now.isNull());
  Clear();
  const bool is_empty = now.isNull();
  if (is_empty) {
    return;
  }
  was_ = Tinted(was, theme::kPick);
  now_ = Tinted(now, Qt::white);
  corner_ = corner;
  glow_ = 1.0;
  animation_ = motion_->Animate(
      this, MotionKind::kMove, 1.0, 0.0, [this](const QVariant& value) {
        glow_ = value.toDouble();
        emit Changed();
      });
  // Off runs no animation: the flash would stay, so it ends at once.
  const bool is_instant = animation_ == nullptr;
  if (is_instant) {
    Clear();
  }
}

void StageFlash::Clear() {
  assert(glow_ >= 0.0);
  assert(glow_ <= 1.5);
  if (animation_) {
    animation_->stop();
  }
  const bool was_playing = IsPlaying();
  glow_ = 0.0;
  was_ = QImage();
  now_ = QImage();
  if (was_playing) {
    emit Changed();
  }
}

void StageFlash::Paint(QPainter* painter) const {
  assert(painter != nullptr);
  assert(glow_ >= 0.0);
  const bool is_idle = !IsPlaying();
  if (is_idle) {
    return;
  }
  painter->save();
  painter->setOpacity(std::clamp(glow_ * kWasShare, 0.0, 1.0));
  painter->drawImage(corner_, was_);
  painter->setOpacity(std::clamp(glow_, 0.0, 1.0));
  painter->drawImage(corner_, now_);
  painter->restore();
}

}  // namespace snapper
