#ifndef SNAPPER_UI_GROUP_BOX_H_
#define SNAPPER_UI_GROUP_BOX_H_

#include <QGroupBox>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <set>
#include <utility>
#include <vector>

#include "edit/group_edits.h"
#include "ui/chip_button.h"
#include "ui/flow_layout.h"
#include "ui/managers.h"

namespace snapper {

// The most chips a shot can show: its own groups and the defaults.
inline constexpr size_t kMaxGroupChips = 64;

// Groups of warp dots, shown while a doll is picked: a chip per group,
// wrapping onto more lines when narrow (click picks its dots, Shift
// adds, Ctrl flips, by the shared pick rule; pressed when all of it is
// picked), a Group button that makes a group of the picked dots, and
// a right-click menu per chip: rename, add or remove the picked dots,
// make it a default for every shot or only this shot's, delete.
// Greyed out with the reason when nothing can act.
class GroupBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit GroupBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  using Names = std::vector<std::pair<QString, bool>>;

  std::set<WarpDot> PickedDots() const;
  // Builds one chip per group of shown, replacing the old ones.
  void Rebuild(const std::vector<ShownGroup>& shown);
  void PickChip(const QString& name);
  void ShowMenu(const QString& name, const QPoint& at);
  void AskName();
  void AskRename(const QString& name);

  Managers managers_;
  QVBoxLayout layout_;
  // Says why no chip shows and how to make one; owned by the box.
  QLabel* hint_ = nullptr;
  QWidget chips_;
  FlowLayout chips_layout_;
  std::vector<ChipButton*> buttons_;
  Names names_;
  QPushButton make_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_GROUP_BOX_H_
