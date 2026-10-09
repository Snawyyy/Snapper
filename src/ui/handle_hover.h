#ifndef SNAPPER_UI_HANDLE_HOVER_H_
#define SNAPPER_UI_HANDLE_HOVER_H_

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantAnimation>

#include "ui/grab.h"

namespace snapper {

class MotionManager;

// Which handle the cursor is over and how far it has grown: 1.0 up to
// kHoverGrow, eased by the Motion manager's hover curve (instant in
// Off), and shrunk back the same way when the cursor leaves. One of these serves each canvas that draws handles, so the
// stage, the rig canvas and the timeline grow them the same way.
class HandleHover final : public QObject {
  Q_OBJECT

 public:
  HandleHover(MotionManager* motion, QObject* parent);

  // The handle now under the cursor; kNone when there is none. The
  // same handle again changes nothing.
  void Set(GrabKind kind, const QString& key);
  // 1.0, or the growth of the handle when it is the hovered one.
  double ScaleOf(GrabKind kind, const QString& key) const;
  GrabKind kind() const { return kind_; }
  const QString& key() const { return key_; }

 signals:
  // The hovered handle or its size changed: repaint.
  void Changed();

 private:
  // Starts the shrink of the handle being left.
  void ShrinkLeft();

  MotionManager* motion_;
  GrabKind kind_ = GrabKind::kNone;
  QString key_;
  double scale_ = 1.0;
  QPointer<QVariantAnimation> animation_;
  // The handle just left, shrinking back on the same curve it grew.
  GrabKind out_kind_ = GrabKind::kNone;
  QString out_key_;
  double out_scale_ = 1.0;
  QPointer<QVariantAnimation> out_animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_HANDLE_HOVER_H_
