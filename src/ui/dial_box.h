#ifndef SNAPPER_UI_DIAL_BOX_H_
#define SNAPPER_UI_DIAL_BOX_H_

#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <memory>
#include <vector>

#include "edit/edit_scope.h"
#include "model/smart_dial.h"
#include "ui/dial_track.h"
#include "ui/managers.h"

namespace snapper {

// The smart dials of the picked doll: for each dial (only those moving a
// picked piece when pieces are picked) a plate with its name over a
// sunken frame holding its track with the stop ticks, and under it what
// it shows ("Front 60% Right") and Set stop, which saves the pose of its
// pieces at the playhead as the stop under the knob; then a "+ Dial"
// tile whose menu makes a dial driving the picked pieces from a
// template. Turning keys the dial on every picked doll that has one of
// the same name. Right click a dial or a tick for the rest of its
// settings. The inspector shows the box only for a doll with dials; a
// doll without any gets its own "+" tile sharing templates().
class DialBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit DialBox(const Managers& managers);
  void Refresh();
  // The template entries, each making a dial; greyed with the reason
  // when no dial can be made from the pick.
  QMenu* templates() { return &templates_; }
  // Why no dial can be made from the pick now; empty when one can.
  QString WhyNoDial() const;

 signals:
  void Problem(const QString& why);

 private:
  // One dial's row.
  struct Row final {
    QString name;
    QWidget* widget = nullptr;
    DialTrack* track = nullptr;
    QLabel* readout = nullptr;
    QPushButton* set_stop = nullptr;
  };

  // The picked pieces of the focused layer.
  std::vector<QString> PickedPieces() const;
  void AddDial(int kind);
  // Builds the rows again when the dials shown changed.
  void Rebuild(const std::vector<SmartDial>& dials);
  void Turn(const QString& dial, double value);
  void SetStop(const QString& dial);
  void ShowMenu(const QString& dial, int stop, QPoint global);
  void Report(const QString& problem);
  QString DollName() const;

  Managers managers_;
  QVBoxLayout layout_;
  QMenu templates_;
  QToolButton add_;
  QVBoxLayout rows_layout_;
  // Says why no dial shows (none moves a picked piece); owned by the box.
  QLabel* empty_ = nullptr;
  std::vector<Row> rows_;
  // The dials the rows were built for.
  std::vector<SmartDial> shown_;
  // A drag of a track: one undo step.
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DIAL_BOX_H_
