#ifndef SNAPPER_UI_LINK_BOX_H_
#define SNAPPER_UI_LINK_BOX_H_

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QString>

#include <vector>

#include "model/shot.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// What the picked things follow, set by right-clicking a leader on the
// stage: whom, from which frame, how strongly, and Unlink. With many
// picked, strength is added to each one's own. Greyed out with the
// reason when nothing picked follows anything.
class LinkBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit LinkBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  std::vector<LinkEnd> Picked() const;
  // The first picked thing's link, the focused layer's first.
  const Link* Shown() const;
  void Commit();

  Managers managers_;
  QFormLayout layout_;
  LiveEdit live_;
  QLabel follows_;
  QDoubleSpinBox strength_;
  QPushButton unlink_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LINK_BOX_H_
