#ifndef SNAPPER_UI_RECORD_BUTTON_H_
#define SNAPPER_UI_RECORD_BUTTON_H_

#include <QString>

#include "ui/managers.h"
#include "ui/tape_key.h"

namespace snapper {

// The Video bar's Record key, a tape key: a red dot and "Rec" when idle, the
// count-in beats, then "Stop" with a level bar while a take records.
// Greyed with the reason when there is no sound input. Everything it
// does goes through RecordManager.
class RecordButton final : public TapeKey {
  Q_OBJECT

 public:
  explicit RecordButton(const Managers& managers);

  // Shows RecordManager's state and why it can't record.
  void Refresh();

 signals:
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  Managers managers_;
  // The loudest sample just heard, drawn under the label.
  double level_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RECORD_BUTTON_H_
