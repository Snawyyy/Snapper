#ifndef SNAPPER_UI_REEL_TEXT_PANEL_H_
#define SNAPPER_UI_REEL_TEXT_PANEL_H_

#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QWidget>

#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "edit/reel_manager.h"
#include "model/reel.h"
#include "ui/choice_row.h"
#include "ui/managers.h"

namespace snapper {

// The look of the picked text clips on the Video tab (with none picked,
// the subtitle track's shared look): font, size, bold, fill, outline
// and box colors from the floating palette, place tiles, how the text
// comes in and goes out, and named styles. Every change goes through
// ReelManager as one named step.
class ReelTextPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit ReelTextPanel(const Managers& managers);

 signals:
  void Problem(const QString& why);

 private:
  // Which color the next palette pick sets.
  enum class ColorPart { kNone, kFill, kOutline, kBox };

  void BuildRows();
  void BuildColorsAndPlace();
  void BuildMotionAndStyle();
  void Wire();
  void WireStyles();
  void Refresh();
  void ShowLook(const ReelTextLook& look);
  void ShowMotion(const std::vector<ClipId>& picked);
  std::vector<ClipId> PickedText() const;
  // What the fields show and edit: the first picked text clip's look,
  // else the subtitle track's; nullopt with neither.
  std::optional<ReelTextLook> ShownLook() const;
  void CommitLook(const QString& label,
                  const std::function<void(ReelTextLook*)>& change);
  void CommitMotion(bool is_appear);
  // A color button: the palette's next pick colors that part (or a
  // color dialog when there is no palette).
  void AimColor(ColorPart part);
  void TakeColor(const QColor& color);
  void SaveStyle();
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  QFormLayout layout_;
  QLabel title_;
  QFontComboBox font_;
  QHBoxLayout size_row_;
  QSpinBox size_;
  QCheckBox bold_;
  QHBoxLayout color_row_;
  QPushButton fill_;
  QPushButton outline_;
  QPushButton box_;
  QPushButton no_box_;
  QSpinBox outline_width_;
  QHBoxLayout spot_row_;
  std::array<QPushButton, kTextSpotCount> spots_;
  QHBoxLayout appear_row_;
  ChoiceRow appear_;
  QSpinBox appear_length_;
  QHBoxLayout leave_row_;
  ChoiceRow leave_;
  QSpinBox leave_length_;
  QHBoxLayout style_row_;
  QComboBox style_;
  QPushButton apply_style_;
  QPushButton save_style_;
  QPushButton remove_style_;
  QCheckBox linked_;
  QCheckBox own_look_;
  ColorPart aimed_ = ColorPart::kNone;
};

}  // namespace snapper

#endif  // SNAPPER_UI_REEL_TEXT_PANEL_H_
