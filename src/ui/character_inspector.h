#ifndef SNAPPER_UI_CHARACTER_INSPECTOR_H_
#define SNAPPER_UI_CHARACTER_INSPECTOR_H_

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include "base/error.h"
#include "model/character.h"
#include "ui/character_session.h"

namespace snapper {

class CharacterManager;
class HistoryManager;

// A square button showing a color; a click opens the color picker.
class SwatchButton final : public QPushButton {
  Q_OBJECT

 public:
  explicit SwatchButton(QWidget* parent = nullptr);
  void SetColor(const QColor& color);
  const QColor& color() const { return color_; }

 signals:
  void Picked(const QColor& color);

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QColor color_ = Qt::white;
};

// The Character tab's right panel: only what the pick needs. The picked
// part's place and binding, the look of new or picked shapes and the
// picked points, and the picked bone with its smart action.
class CharacterInspector final : public QWidget {
  Q_OBJECT

 public:
  CharacterInspector(HistoryManager* history, CharacterManager* characters,
                     CharacterSession* session, QWidget* parent = nullptr);

 signals:
  void Problem(const QString& message);

 private:
  QWidget* BuildPart();
  QWidget* BuildLook();
  QWidget* BuildBone();
  QWidget* BuildAction();
  void Refresh();
  void RefreshPart(const Character& character, const CharacterPart& part);
  void RefreshLook(const CharacterPart* part);
  void RefreshBone(const Character& character, const Bone& bone);
  void CommitPart();
  void CommitLook();
  void CommitBone();
  void Report(const Result<void>& done);
  const Character* Shown() const;

  HistoryManager* history_;
  CharacterManager* characters_;
  CharacterSession* session_;
  bool is_filling_ = false;

  QWidget* part_section_ = nullptr;
  QFormLayout* part_form_ = nullptr;
  QLabel part_kind_;
  QDoubleSpinBox x_;
  QDoubleSpinBox y_;
  QDoubleSpinBox turn_;
  QDoubleSpinBox scale_x_;
  QDoubleSpinBox scale_y_;
  QDoubleSpinBox opacity_;
  QCheckBox flipped_;
  QComboBox bind_;
  QComboBox bind_bone_;
  QLabel pick_label_;
  QComboBox rest_pick_;

  QWidget* look_section_ = nullptr;
  QLabel look_for_;
  QCheckBox has_fill_;
  SwatchButton fill_;
  QCheckBox has_stroke_;
  SwatchButton stroke_;
  QDoubleSpinBox stroke_width_;
  QPushButton make_shape_;
  QPushButton raise_;
  QPushButton lower_;
  QDoubleSpinBox curvature_;
  QDoubleSpinBox point_width_;
  QPushButton weld_;

  QWidget* bone_section_ = nullptr;
  QLabel bone_name_;
  QDoubleSpinBox strength_;
  QCheckBox has_limits_;
  QDoubleSpinBox min_turn_;
  QDoubleSpinBox max_turn_;
  QCheckBox locked_;
  QDoubleSpinBox dynamics_;
  QCheckBox smart_;

  QWidget* action_section_ = nullptr;
  QSlider action_turn_;
  QLabel action_turn_text_;
  QListWidget stops_;
  QPushButton add_stop_;
  QPushButton pose_stop_;
  QPushButton remove_stop_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CHARACTER_INSPECTOR_H_
