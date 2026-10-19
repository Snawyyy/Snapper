#ifndef SNAPPER_UI_CHARACTER_BOX_H_
#define SNAPPER_UI_CHARACTER_BOX_H_

#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSlider>
#include <QString>

#include <memory>
#include <vector>

#include "base/error.h"
#include "edit/edit_scope.h"
#include "model/character.h"
#include "ui/managers.h"

namespace snapper {

// A character picked whole on the Pose tab: what each switch shows and
// each smart bone's turn at the playhead (setting either keys the whole
// pose there), Reset pose and Flip. The inspector shows it only for
// characters.
class CharacterBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit CharacterBox(const Managers& managers);
  void Refresh();

 signals:
  void Problem(const QString& why);

 private:
  // The character the focused layer shows, or nullptr.
  const Character* Shown() const;
  void Rebuild(const Character& character);
  void Report(const Result<void>& done);

  Managers managers_;
  QFormLayout layout_;
  QPushButton reset_;
  QPushButton flip_;
  // What the rows were built for, so a refresh that changes nothing in
  // the rig keeps the widgets (and a drag on a slider) alive.
  QString built_for_;
  std::vector<PartId> switches_;
  std::vector<BoneId> smart_bones_;
  std::vector<QWidget*> rows_;
  std::unique_ptr<EditScope> scope_;
  bool is_filling_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_BOX_H_
