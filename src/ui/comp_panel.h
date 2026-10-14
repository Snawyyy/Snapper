#ifndef SNAPPER_UI_COMP_PANEL_H_
#define SNAPPER_UI_COMP_PANEL_H_

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

#include "base/frame.h"
#include "edit/comp_manager.h"
#include "model/comp.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// The open body of one effect's fold in the Comp tab's effect list:
// blend (a sunken dropdown in groups), the target colors, the effect's
// color, its look tiles, gradient shapes and stops, color pairs, and a
// fader or dial per keyed knob with a Key button. Colors come from the
// floating palette: click a color button (which picks the effect), then
// a swatch. Every control calls CompManager; a drag is one undo step
// and Escape cancels it.
class CompPanel final : public QWidget {
  Q_OBJECT

 public:
  CompPanel(const Managers& managers, ShotId shot, CompEffectId effect,
            QWidget* parent);

 signals:
  void Problem(const QString& why);

 private:
  // What a palette pick goes to; kKnobColor is a plugin's color knob
  // (aim_index_ is the knob).
  enum class Aim {
    kNone,
    kColor,
    kTarget,
    kStop,
    kSwapFrom,
    kSwapTo,
    kKnobColor
  };

  // One built control row; only the widgets its kind uses are set.
  struct KnobRow final {
    CompKnob knob = CompKnob::kAmount;
    QDoubleSpinBox* box = nullptr;
    QPushButton* key = nullptr;
    QCheckBox* toggle = nullptr;
    QComboBox* choice = nullptr;
  };

  void Report(const QString& problem) {
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  }
  void BuildFixed();
  void Refresh();
  // Rebuilds the controls that depend on the effect's kind and look.
  void Rebuild(const CompEffect* effect);
  void BuildLook(const CompEffect& effect, QVBoxLayout* into);
  void BuildGradient(const CompEffect& effect, QVBoxLayout* into);
  void BuildSwaps(const CompEffect& effect, QVBoxLayout* into);
  // The rows CompManager::RowsOf names (ui/comp_panel_knobs.cpp).
  void BuildKnobs(const CompEffect& effect, QVBoxLayout* into);
  void AddNumberRow(const CompRow& row, const CompEffect& effect,
                    QFormLayout* form);
  void AddToggleRow(const CompRow& row, QFormLayout* form);
  void AddChoiceRow(const CompRow& row, QFormLayout* form);
  void AddColorRow(const CompRow& row, const CompEffect& effect,
                   QFormLayout* form);
  // Shows the knobs' values and keys at the playhead.
  void ShowValues(const CompEffect& effect);
  void ShowRow(const KnobRow& row, const CompEffect& effect, Frame local);
  void AimAt(Aim aim, int index);
  void TakeColor(const QColor& color, Qt::KeyboardModifiers modifiers);
  QPushButton* Swatch(const QColor& color, const QString& tip,
                      QWidget* parent);
  // What the controls are built for; a change rebuilds them.
  QString Signature(const CompEffect* effect) const;
  const CompEffect* Effect() const;
  ShotId Shot() const;
  Frame Local() const;

  Managers managers_;
  LiveEdit live_;
  QVBoxLayout layout_;
  ShotId shot_;
  CompEffectId effect_;
  // Why the effect draws nothing (a missing plugin, or one that refused
  // to run); hidden while it runs.
  QLabel note_;
  QComboBox blend_;
  QHBoxLayout colors_row_;
  QPushButton pick_colors_;
  QPushButton clear_colors_;
  QWidget* dynamic_ = nullptr;
  QString signature_;
  std::vector<KnobRow> knob_rows_;
  QCheckBox* eased_ = nullptr;
  Aim aim_ = Aim::kNone;
  int aim_index_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_PANEL_H_
