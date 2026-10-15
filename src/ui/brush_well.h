#ifndef SNAPPER_UI_BRUSH_WELL_H_
#define SNAPPER_UI_BRUSH_WELL_H_

#include <QSize>
#include <QWidget>

#include "ui/managers.h"

class QPaintEvent;

namespace snapper {

// Under draw mode's tool keys: a small display showing the brush as it
// will land, a dot of its size in the current color, with its size in
// pixels glowing under it.
class BrushWell final : public QWidget {
  Q_OBJECT

 public:
  explicit BrushWell(const Managers& managers);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  Managers managers_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_BRUSH_WELL_H_
