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
// Off). One of these serves each canvas that draws handles, so the
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
  MotionManager* motion_;
  GrabKind kind_ = GrabKind::kNone;
  QString key_;
  double scale_ = 1.0;
  QPointer<QVariantAnimation> animation_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_HANDLE_HOVER_H_
