#ifndef SNAPPER_UI_KEY_CHIP_BAR_H_
#define SNAPPER_UI_KEY_CHIP_BAR_H_

#include <QHBoxLayout>
#include <QPoint>
#include <QPushButton>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QWidget>

#include <array>

#include "model/key.h"
#include "ui/managers.h"

namespace snapper {

// The bar's three eases, in the order they show.
inline constexpr std::array<Ease, 3> kChipEases = {
    Ease::kStep, Ease::kEaseInOut, Ease::kLinear};

// The chips that float above the picked keys: how the pose moves on to
// the next key (Step, Ease, Line), on 2s and on 3s, and a hold one
// frame shorter or longer. They make the same KeyManager calls as the
// right-click menu. A chip that can't act is greyed out and says why;
// the Ease that all the picked keys share shows pressed.
class KeyChipBar final : public QWidget {
  Q_OBJECT

 public:
  KeyChipBar(const Managers& managers, QWidget* parent);

  // Shows what the picked keys allow, and hides the bar with none.
  void Refresh();

  // Where the bar's top left goes: centred over keys (their box, in
  // area's coordinates), just above them, or just below when there is
  // no room above, and always inside area.
  static QPoint SpotFor(const QRectF& keys, QSize bar, const QRectF& area);

 signals:
  void Problem(const QString& why);

 private:
  QPushButton* AddChip(const QString& text, const QString& name);

  Managers managers_;
  QHBoxLayout layout_;
  std::array<QPushButton*, kChipEases.size()> eases_{};
  QPushButton* on_two_ = nullptr;
  QPushButton* on_three_ = nullptr;
  QPushButton* shorter_ = nullptr;
  QPushButton* longer_ = nullptr;
};

}  // namespace snapper

#endif  // SNAPPER_UI_KEY_CHIP_BAR_H_
