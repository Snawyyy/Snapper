#ifndef SNAPPER_UI_BUMP_LABEL_H_
#define SNAPPER_UI_BUMP_LABEL_H_

#include <QLabel>
#include <QPointer>
#include <QVariantAnimation>

namespace snapper {

class MotionManager;

// A label whose text hops up a few pixels and settles back when it
// changes, through the Pop time (it never moves in Off). It keeps its
// size: the hop only shifts the text inside the label.
class BumpLabel final : public QLabel {
  Q_OBJECT

 public:
  explicit BumpLabel(MotionManager* motion);

  // Sets the text; when it is new and is_bumping, the text hops.
  void SetText(const QString& text, bool is_bumping);
  // How far the text is raised right now, in pixels.
  int lift() const { return lift_; }

 private:
  void SetLift(int lift);
  MotionManager* motion_;
  QPointer<QVariantAnimation> animation_;
  int lift_ = 0;
};

// How high the hop starts, in pixels.
inline constexpr int kBumpPx = 6;

}  // namespace snapper

#endif  // SNAPPER_UI_BUMP_LABEL_H_
