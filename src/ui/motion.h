#ifndef SNAPPER_UI_MOTION_H_
#define SNAPPER_UI_MOTION_H_

#include <QEasingCurve>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantAnimation>

#include <functional>

#include "ui/motion_table.h"

namespace snapper {

// The one place that decides how long and how softly the interface
// moves. Every animation asks it, so the Motion switch changes them all
// at once. The mode is saved in the settings file.
//
// Qt has no way to ask whether the system wants reduced motion, so a
// fresh install starts in Full.
class MotionManager final : public QObject {
  Q_OBJECT

 public:
  // settings_path is the settings file the mode is read from and saved to.
  explicit MotionManager(const QString& settings_path);

  MotionMode mode() const { return mode_; }
  // Saves the mode and tells everyone at once; no restart.
  void SetMode(MotionMode mode);

  int duration(MotionKind kind) const { return MotionMs(mode_, kind); }
  QEasingCurve curve(MotionKind kind) const {
    return MotionEasing(mode_, kind);
  }

  // Runs from to to, timed by kind, calling on_value with every value
  // (the last one is always to). In Off it calls once with to, at once,
  // and returns nullptr. Otherwise parent owns the animation, which
  // deletes itself when done.
  QVariantAnimation* Animate(
      QObject* parent, MotionKind kind, const QVariant& from,
      const QVariant& to,
      const std::function<void(const QVariant&)>& on_value) const;

 signals:
  void ModeChanged(MotionMode mode);

 private:
  QString settings_path_;
  MotionMode mode_ = MotionMode::kFull;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MOTION_H_
