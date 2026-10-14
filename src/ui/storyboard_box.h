#ifndef SNAPPER_UI_STORYBOARD_BOX_H_
#define SNAPPER_UI_STORYBOARD_BOX_H_

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include "model/shot.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The picked storyboard layer, in the panel kit's look: its opacity
// fader, the In export tick, drawing a new frame at the playhead or
// over the one showing, and deleting the picked frames. The layer's eye
// and its draw order are the layer list's and the timeline row's. Each
// control is greyed with the reason when it can't act. Every change
// goes through the StoryboardManager.
class StoryboardBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit StoryboardBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  ShotId Shot() const;
  // The storyboard layer the box edits, invalid when none is picked.
  LayerId Layer() const;

  Managers managers_;
  LiveEdit live_;
  QVBoxLayout layout_;
  QWidget form_host_;
  QCheckBox exported_;
  QDoubleSpinBox opacity_;
  QPushButton draw_new_;
  QPushButton draw_over_;
  QPushButton remove_;
  QLabel count_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_STORYBOARD_BOX_H_
