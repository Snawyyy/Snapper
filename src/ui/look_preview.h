#ifndef SNAPPER_UI_LOOK_PREVIEW_H_
#define SNAPPER_UI_LOOK_PREVIEW_H_

#include <QImage>
#include <QSize>
#include <QWidget>

#include "model/doll.h"

class QPaintEvent;

namespace snapper {

// A small screen showing what a look does: a tiny pixel drawing turned
// and blown up as Crisp (square steps), Smooth (blended) or Smooth flat
// (blended edges, flat fills), with hard edges cut out when asked.
class LookPreview final : public QWidget {
  Q_OBJECT

 public:
  LookPreview();

  void Show(DrawingLook look, bool has_hard_edges);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  // The drawing shown: a few pixels, as a pixel artist would draw it.
  QImage sprite_;
  DrawingLook look_ = DrawingLook::kCrisp;
  bool has_hard_edges_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LOOK_PREVIEW_H_
