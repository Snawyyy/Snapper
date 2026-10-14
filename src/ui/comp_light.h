#ifndef SNAPPER_UI_COMP_LIGHT_H_
#define SNAPPER_UI_COMP_LIGHT_H_

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "base/frame.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The shot's Light under the Comp tab's effect list: a dial and a field
// for where the light comes from, and a Key button, for every rim light
// and gradient that follows it.
class CompLight final : public QWidget {
  Q_OBJECT

 public:
  explicit CompLight(const Managers& managers);

 signals:
  void Problem(const QString& why);

 private:
  void Report(const QString& problem) {
    const bool has_problem = !problem.isEmpty();
    if (has_problem) {
      emit Problem(problem);
    }
  }
  void Refresh();
  Frame Local() const;

  Managers managers_;
  LiveEdit live_;
  QHBoxLayout layout_;
  QDoubleSpinBox box_;
  QPushButton key_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_LIGHT_H_
