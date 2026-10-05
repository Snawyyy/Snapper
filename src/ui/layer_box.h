#ifndef SNAPPER_UI_LAYER_BOX_H_
#define SNAPPER_UI_LAYER_BOX_H_

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>

#include "model/layer.h"
#include "ui/managers.h"

namespace snapper {

// The picked layer's own settings: name and timing for any layer, flip
// for a doll, words and style for text, kind, colour and strength for
// an effect. Rows that don't apply to the layer's kind are hidden.
class LayerBox final : public QGroupBox {
  Q_OBJECT

 public:
  explicit LayerBox(const Managers& managers);
  void Refresh();

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;

 signals:
  void Problem(const QString& why);

 private:
  void BuildRows();
  void Wire();
  void CommitName();
  void CommitTiming();
  void CommitText();
  void CommitEffect();
  void CommitStrength();
  const Layer* Picked() const;
  void ShowRows(bool is_doll, bool is_text, bool is_effect);

  Managers managers_;
  QFormLayout layout_;
  QLineEdit name_;
  QSpinBox start_;
  QSpinBox length_;
  QCheckBox flip_;
  QPlainTextEdit words_;
  QDoubleSpinBox size_;
  QCheckBox bold_;
  QPushButton fill_;
  QPushButton outline_;
  QDoubleSpinBox outline_width_;
  QComboBox effect_;
  QPushButton effect_colour_;
  QDoubleSpinBox strength_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LAYER_BOX_H_
