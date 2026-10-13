#include "ui/plated_box.h"

#include <cassert>

#include "ui/panel_kit.h"

namespace snapper {

QLabel* PlateBox(QWidget* box, const QString& title, QLayout* body) {
  assert(box != nullptr && body != nullptr);
  assert(box->layout() == nullptr);
  auto* inside = MakeOwned<QWidget>(box);
  inside->setLayout(body);
  body->setContentsMargins(0, 0, 0, 0);
  QWidget* section = PlatedSection(title, inside, box);
  StackIn(box)->addWidget(section);
  auto* plate = section->findChild<QLabel*>(QStringLiteral("plate"));
  assert(plate != nullptr);
  return plate;
}

}  // namespace snapper
