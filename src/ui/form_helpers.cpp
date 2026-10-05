#include "ui/form_helpers.h"

#include <QColorDialog>
#include <QSignalBlocker>

#include <cassert>
#include <cmath>

namespace snapper {

void SetUpNumber(QDoubleSpinBox* box, double low, double high, int decimals,
                 const QString& suffix) {
  assert(box != nullptr);
  assert(low < high && decimals >= 0);
  box->setRange(low, high);
  box->setDecimals(decimals);
  box->setSuffix(suffix);
  box->setKeyboardTracking(false);
  box->setAccelerated(true);
}

void ShowNumber(QDoubleSpinBox* box, double value) {
  assert(box != nullptr);
  assert(std::isfinite(value));
  const QSignalBlocker quiet(box);
  box->setValue(value);
}

void ShowColour(QPushButton* button, const QColor& colour) {
  assert(button != nullptr);
  assert(colour.isValid());
  button->setText(colour.name(QColor::HexArgb));
  button->setStyleSheet(QStringLiteral("background: %1; color: %2;")
                            .arg(colour.name(),
                                 colour.lightness() > 128
                                     ? QStringLiteral("black")
                                     : QStringLiteral("white")));
}

QColor AskColour(QWidget* parent, const QColor& current) {
  assert(parent != nullptr);
  assert(current.isValid());
  return QColorDialog::getColor(current, parent, QString(),
                                QColorDialog::ShowAlphaChannel);
}

void Explain(QWidget* widget, const QString& why_not) {
  assert(widget != nullptr);
  assert(why_not.size() < 100000);
  widget->setEnabled(why_not.isEmpty());
  widget->setToolTip(why_not);
}

}  // namespace snapper
