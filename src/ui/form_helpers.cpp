#include "ui/form_helpers.h"

#include <QColorDialog>
#include <QSignalBlocker>

#include <cassert>
#include <cmath>

namespace snapper {

void SetUpNumber(QDoubleSpinBox* box, Number kind, const QString& prefix) {
  assert(box != nullptr);
  assert(prefix.size() < 16);
  struct Shape final {
    double low;
    double high;
    double step;
    int decimals;
    const char* suffix;
  };
  const auto shape = [kind]() -> Shape {
    switch (kind) {
      case Number::kPixels:
        return {-100000, 100000, 1, 1, " px"};
      case Number::kSize:
        return {0, 10000, 1, 1, " px"};
      case Number::kDegrees:
        return {-3600, 3600, 1, 1, " deg"};
      case Number::kLean:
        return {-85, 85, 1, 1, " deg"};
      case Number::kScale:
        return {0, 10, 0.05, 2, ""};
      case Number::kFraction:
        return {0, 1, 0.05, 2, ""};
      case Number::kZoom:
        return {0.05, 10, 0.05, 2, ""};
    }
    return {0, 1, 0.05, 2, ""};
  }();
  box->setRange(shape.low, shape.high);
  box->setSingleStep(shape.step);
  box->setDecimals(shape.decimals);
  box->setSuffix(QObject::tr(shape.suffix));
  box->setPrefix(prefix);
  box->setAccelerated(true);
  box->setMinimumWidth(64);
}

void AddTitle(QFormLayout* form, QLabel* title, const QString& text) {
  assert(form != nullptr && title != nullptr);
  assert(!text.isEmpty());
  QFont bold = title->font();
  bold.setBold(true);
  title->setFont(bold);
  title->setText(text);
  form->addRow(title);
}

void AddPair(QFormLayout* form, const QString& label, QHBoxLayout* row,
             QWidget* first, QWidget* second) {
  assert(form != nullptr && row != nullptr);
  assert(first != nullptr && second != nullptr);
  row->setSpacing(4);
  row->addWidget(first, 1);
  row->addWidget(second, 1);
  form->addRow(label, row);
}

void ShowNumber(QDoubleSpinBox* box, double value) {
  assert(box != nullptr);
  assert(std::isfinite(value));
  // Never rewrite a field while it is being typed in.
  const bool is_typing = box->hasFocus();
  if (is_typing) {
    return;
  }
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
