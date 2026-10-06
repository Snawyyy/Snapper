#ifndef SNAPPER_UI_SHOT_BOX_H_
#define SNAPPER_UI_SHOT_BOX_H_

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>

#include <vector>

#include "model/shot.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The picked shots' settings: name, length, background, and how each
// hands over to the next. Shows the focused shot; changes go to every
// picked shot (lengths by the same difference).
class ShotBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit ShotBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  const Shot* Focused() const;
  std::vector<ShotId> Picked() const;
  void PickBackground();
  void SetTransition(TransitionKind kind, int frames);

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  QLineEdit name_;
  QSpinBox length_;
  QPushButton background_;
  QComboBox transition_;
  QSpinBox transition_length_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_BOX_H_
