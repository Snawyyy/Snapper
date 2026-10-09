#ifndef SNAPPER_UI_TIMING_CHART_DIALOG_H_
#define SNAPPER_UI_TIMING_CHART_DIALOG_H_

#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <vector>

#include "ui/managers.h"
#include "ui/timing_chart_strip.h"

namespace snapper {

// The small floating window for the timing chart: the chart, a spacing
// preset, which frames get keys (remembered between sessions), Apply
// and Cancel. Enter applies, Escape closes (Escape first
// cancels a tick drag). It opens when the manager opens a chart and
// closes when it closes; the stage shows the result live meanwhile.
class TimingChartDialog final : public QDialog {
  Q_OBJECT

 public:
  TimingChartDialog(const Managers& managers, QWidget* parent);

  void accept() override;
  void reject() override;

  TimingChartStrip* strip() { return &strip_; }
  QComboBox* presets() { return &presets_; }
  QComboBox* key_modes() { return &key_modes_; }
  QPushButton* apply_button() { return &apply_; }

 private:
  void BuildLayout();
  void Show();
  // Makes the window show what the manager holds: the preset, whether
  // Apply can act and why not.
  void Refresh();
  void PickKeyMode(int index);
  // Glides the ticks to the preset, in the Move time.
  void PickPreset(int index);

  Managers managers_;
  QVBoxLayout layout_;
  QLabel title_;
  TimingChartStrip strip_;
  QHBoxLayout options_;
  QHBoxLayout key_options_;
  QLabel keys_label_;
  QComboBox key_modes_;
  QLabel spacing_label_;
  QComboBox presets_;
  QHBoxLayout buttons_;
  QPushButton apply_;
  QPushButton cancel_;
  QPointer<QVariantAnimation> glide_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMING_CHART_DIALOG_H_
