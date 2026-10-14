#ifndef SNAPPER_UI_VOICE_MODE_TILE_H_
#define SNAPPER_UI_VOICE_MODE_TILE_H_

#include "ui/loop_tile.h"

namespace snapper {

// A Voice panel mode tile. Clone: two speech boxes, the copy sliding
// out from behind the first. Design: a sparkle twinkling over a pencil
// line. The picked mode wears the Teto red outline.
class VoiceModeTile final : public LoopTile {
  Q_OBJECT

 public:
  VoiceModeTile(bool is_design, const QString& label, MotionManager* motion,
                QWidget* parent);

  QSize sizeHint() const override;

 protected:
  void PaintArt(QPainter* painter, const QRectF& art, double phase,
                const QColor& ink) const override;

 private:
  bool is_design_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VOICE_MODE_TILE_H_
