#include "ui/motion.h"

#include <QSettings>

#include <cassert>
#include <memory>

namespace snapper {
namespace {

const char* const kModeKey = "motion";

}  // namespace

MotionManager::MotionManager(const QString& settings_path)
    : settings_path_(settings_path) {
  assert(!settings_path.isEmpty());
  const QSettings settings(settings_path_, QSettings::IniFormat);
  const QString saved = settings.value(kModeKey).toString();
  mode_ = MotionModeFromKey(saved, MotionMode::kFull);
  assert(mode_ >= MotionMode::kFull && mode_ <= MotionMode::kOff);
}

void MotionManager::SetMode(MotionMode mode) {
  assert(mode >= MotionMode::kFull && mode <= MotionMode::kOff);
  const bool is_same = mode == mode_;
  if (is_same) {
    return;
  }
  mode_ = mode;
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.setValue(kModeKey, MotionModeKey(mode_));
  emit ModeChanged(mode_);
  assert(mode_ == mode);
}

QVariantAnimation* MotionManager::Animate(
    QObject* parent, MotionKind kind, const QVariant& from,
    const QVariant& to,
    const std::function<void(const QVariant&)>& on_value) const {
  assert(parent != nullptr);
  assert(from.isValid() && to.isValid());
  // Qt runs a zero-length animation without any value, so Off lands
  // here by hand.
  const int ms = duration(kind);
  const bool is_instant = ms == 0;
  if (is_instant) {
    on_value(to);
    return nullptr;
  }
  auto animation = std::make_unique<QVariantAnimation>(parent);
  animation->setStartValue(from);
  animation->setEndValue(to);
  animation->setDuration(ms);
  animation->setEasingCurve(curve(kind));
  QObject::connect(animation.get(), &QVariantAnimation::valueChanged,
                   parent, on_value);
  animation->start(QAbstractAnimation::DeleteWhenStopped);
  return animation.release();
}

}  // namespace snapper
