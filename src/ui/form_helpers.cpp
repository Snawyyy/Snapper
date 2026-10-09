#include "ui/form_helpers.h"

#include <QColorDialog>
#include <QLineEdit>
#include <QSignalBlocker>

#include <cassert>
#include <cmath>
#include <memory>

#include "base/number_text.h"
#include "ui/number_input.h"
#include "ui/scrub_label.h"

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
    double scrub_unit;
  };
  const auto shape = [kind]() -> Shape {
    switch (kind) {
      case Number::kPixels:
        return {-100000, 100000, 1, 1, " px", 1};
      case Number::kSize:
        return {0, 10000, 1, 1, " px", 1};
      case Number::kDegrees:
        return {-3600, 3600, 1, 1, " deg", 1};
      case Number::kLean:
        return {-85, 85, 1, 1, " deg", 1};
      case Number::kScale:
        return {0, 10, 0.05, 2, "", 0.01};
      case Number::kFraction:
        return {0, 1, 0.05, 2, "", 0.01};
      case Number::kZoom:
        return {0.05, 10, 0.05, 2, "", 0.01};
    }
    return {0, 1, 0.05, 2, "", 0.01};
  }();
  box->setRange(shape.low, shape.high);
  box->setSingleStep(shape.step);
  box->setDecimals(shape.decimals);
  box->setSuffix(QObject::tr(shape.suffix));
  box->setPrefix(prefix);
  box->setAccelerated(true);
  box->setMinimumWidth(64);
  SetScrubUnit(box, shape.scrub_unit);
}

void SetScrubUnit(QAbstractSpinBox* box, double unit) {
  assert(box != nullptr);
  assert(unit > 0.0);
  box->setProperty("scrub_unit", unit);
}

void AddNumberRow(QFormLayout* form, const QString& label,
                  QAbstractSpinBox* box, LiveEdit* live) {
  assert(form != nullptr && box != nullptr);
  assert(!label.isEmpty());
  NumberInput::Install(box);
  // The form takes the label as its child, like a plain row's label.
  auto scrub = std::make_unique<ScrubLabel>(label, box, live);
  form->addRow(scrub.release(), box);
}

void AddNumberPair(QFormLayout* form, const QString& label, QHBoxLayout* row,
                   const NumberPart& first, const NumberPart& second,
                   LiveEdit* live) {
  assert(form != nullptr && row != nullptr);
  assert(first.box != nullptr && second.box != nullptr);
  row->setSpacing(4);
  for (const NumberPart* part : {&first, &second}) {
    assert(part->box->property("prefix").toString().isEmpty());
    NumberInput::Install(part->box);
    auto scrub = std::make_unique<ScrubLabel>(part->text, part->box, live);
    row->addWidget(scrub.release());
    row->addWidget(part->box, 1);
  }
  form->addRow(label, row);
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

void ShowNumber(QSpinBox* box, int value) {
  assert(box != nullptr);
  assert(box->maximum() >= box->minimum());
  const bool is_typing = box->hasFocus();
  if (is_typing) {
    return;
  }
  const QSignalBlocker quiet(box);
  box->setValue(value);
}

void ShowMixed(QAbstractSpinBox* box) {
  assert(box != nullptr);
  assert(box->findChild<QLineEdit*>() != nullptr);
  const bool is_typing = box->hasFocus();
  if (is_typing) {
    return;
  }
  // The box's own text slot would take "Mixed" for typing.
  QLineEdit* edit = box->findChild<QLineEdit*>();
  const QSignalBlocker quiet_box(box);
  const QSignalBlocker quiet_edit(edit);
  edit->setText(QObject::tr("Mixed"));
}

void ShowNumber(QDoubleSpinBox* box, double focus_value,
                const std::vector<double>& picked) {
  assert(box != nullptr);
  assert(std::isfinite(focus_value));
  ShowNumber(box, focus_value);
  const bool is_mixed = HasMixedValues(picked);
  if (is_mixed) {
    ShowMixed(box);
  }
}

void ShowNumber(QSpinBox* box, int focus_value,
                const std::vector<double>& picked) {
  assert(box != nullptr);
  assert(box->maximum() >= box->minimum());
  ShowNumber(box, focus_value);
  const bool is_mixed = HasMixedValues(picked);
  if (is_mixed) {
    ShowMixed(box);
  }
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

QString TimeText(Frame frame) {
  assert(frame.index() >= 0);
  assert(kFramesPerSecond > 0);
  const int seconds = frame.index() / kFramesPerSecond;
  return QStringLiteral("%1:%2")
      .arg(seconds / 60)
      .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void Explain(QWidget* widget, const QString& why_not) {
  assert(widget != nullptr);
  assert(why_not.size() < 100000);
  widget->setEnabled(why_not.isEmpty());
  widget->setToolTip(why_not);
}

void Explain(QWidget* widget, const QString& why_not, const QString& help) {
  assert(widget != nullptr);
  assert(!help.isEmpty());
  Explain(widget, why_not);
  const bool is_usable = why_not.isEmpty();
  if (is_usable) {
    widget->setToolTip(help);
  }
}

void Explain(QAction* action, const QString& why_not) {
  assert(action != nullptr);
  assert(why_not.size() < 100000);
  action->setEnabled(why_not.isEmpty());
  action->setToolTip(why_not);
}

}  // namespace snapper
