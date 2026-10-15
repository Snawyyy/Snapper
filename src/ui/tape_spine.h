#ifndef SNAPPER_UI_TAPE_SPINE_H_
#define SNAPPER_UI_TAPE_SPINE_H_

#include <QAbstractButton>
#include <QSize>
#include <QString>

class QPaintEvent;

namespace snapper {

// A tape on a rack, seen by its spine: a black shell with a cream label
// carrying its name, a red band at its end. A checked one is pulled out
// of the rack: it stands proud, its band lit.
class TapeSpine final : public QAbstractButton {
  Q_OBJECT

 public:
  explicit TapeSpine(const QString& name);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TAPE_SPINE_H_
