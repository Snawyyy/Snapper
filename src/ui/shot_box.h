#ifndef SNAPPER_UI_SHOT_BOX_H_
#define SNAPPER_UI_SHOT_BOX_H_

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

// The picked shots' settings: name, length and background. Shows the
// focused shot; changes go to every picked shot (lengths by the same
// difference). Transitions live on the reel's cuts (Video tab).
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

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  QLineEdit name_;
  QSpinBox length_;
  QPushButton background_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_BOX_H_
