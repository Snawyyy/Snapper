#ifndef SNAPPER_UI_DRAFT_BOX_H_
#define SNAPPER_UI_DRAFT_BOX_H_

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

// The picked shot's draft, in the panel kit's look: its eye, its
// opacity fader, drawing a new cel at the playhead or over the one
// showing, and deleting the picked cels. Each button is greyed with the
// reason when it can't act. Self-contained, for a panel to slot in;
// every change goes through the DraftManager.
class DraftBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit DraftBox(const Managers& managers);
  void Refresh();

 private:
  ShotId Shot() const;
  void Report(const QString& problem) const;

  Managers managers_;
  LiveEdit live_;
  QVBoxLayout layout_;
  QWidget form_host_;
  QCheckBox shown_;
  QDoubleSpinBox opacity_;
  QPushButton draw_new_;
  QPushButton draw_over_;
  QPushButton remove_;
  QLabel count_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DRAFT_BOX_H_
