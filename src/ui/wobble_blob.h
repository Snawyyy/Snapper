#ifndef SNAPPER_UI_WOBBLE_BLOB_H_
#define SNAPPER_UI_WOBBLE_BLOB_H_

#include <QSize>
#include <QTimer>
#include <QWidget>

#include "anim/hop_preview.h"
#include "model/doll.h"

class QPaintEvent;
class QShowEvent;
class QHideEvent;

namespace snapper {

class MotionManager;

// A small live preview of the picked dot's settings: a blob that hops
// on a floor and lands, its edge moved by the same solver the stage
// runs (HopPreview), so a firmer, wobblier or squishier dot looks it.
// Still rests. It runs only while shown and enabled and never in Off,
// where it shows the blob resting.
class WobbleBlob final : public QWidget {
  Q_OBJECT

 public:
  WobbleBlob(MotionManager* motion, QWidget* parent);

  // The settings shown; nullptr is Still.
  void SetNode(const DragNode* node);
  bool is_animating() const { return timer_.isActive(); }
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void hideEvent(QHideEvent* event) override;
  void changeEvent(QEvent* event) override;

 private:
  // Starts or stops the clock to match shown, enabled and Motion.
  void Sync();

  MotionManager* motion_;
  QTimer timer_;
  HopPreview hop_{nullptr};
};

}  // namespace snapper

#endif  // SNAPPER_UI_WOBBLE_BLOB_H_
