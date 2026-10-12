#ifndef SNAPPER_UI_DIAL_BOX_H_
#define SNAPPER_UI_DIAL_BOX_H_

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <memory>
#include <vector>

#include "edit/edit_scope.h"
#include "model/smart_dial.h"
#include "ui/dial_track.h"
#include "ui/managers.h"

namespace snapper {

// The smart dials of the picked doll: template tiles that make a dial
// driving the picked pieces, and for each dial (only those moving a
// picked piece when pieces are picked) its name, its track with the
// stop ticks, what it shows ("Front 60% Right") and Set stop, which
// saves the pose of its pieces at the playhead as the stop under the
// knob. Turning keys the dial on every picked doll that has one of the
// same name. Right click a dial or a tick for the rest of its settings.
class DialBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit DialBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  // One dial's row.
  struct Row final {
    QString name;
    QWidget* widget = nullptr;
    QLabel* title = nullptr;
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
  QHBoxLayout tiles_;
  std::vector<QPushButton*> tile_buttons_;
  QVBoxLayout rows_layout_;
  QLabel empty_;
  std::vector<Row> rows_;
  // The dials the rows were built for.
  std::vector<SmartDial> shown_;
  // A drag of a track: one undo step.
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DIAL_BOX_H_
