#ifndef SNAPPER_UI_KIND_TILE_H_
#define SNAPPER_UI_KIND_TILE_H_

#include <QAbstractButton>
#include <QSize>
#include <QTimer>

#include "ui/dot_kinds.h"
#include "ui/hover_fade.h"

class QEnterEvent;
class QEvent;
class QPaintEvent;

namespace snapper {

class MotionManager;

// One material tile: a tiny cartoon of the material over its name. The
// cartoon sits still, and loops only while the pointer is over the tile
// (never in Off). A press squashes the tile and it springs back. The
// tile of a used material has a Teto red outline, thick when every
// picked dot is of it, thin when only some are.
class KindTile final : public QAbstractButton {
  Q_OBJECT

 public:
  KindTile(Material material, MotionManager* motion, QWidget* parent);

  Material material() const { return material_; }
  void SetShare(Share share);
  Share share() const { return share_; }
  // Whether the cartoon is looping right now.
  bool is_animating() const { return timer_.isActive(); }
  // Where the loop is, 0 to 1.
  double phase() const { return phase_; }
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  void Step();

  Material material_;
  MotionManager* motion_;
  HoverFade* fade_;
  QTimer timer_;
  double phase_;
  Share share_ = Share::kNone;
};

}  // namespace snapper

#endif  // SNAPPER_UI_KIND_TILE_H_
