#ifndef SNAPPER_UI_FORM_HELPERS_H_
#define SNAPPER_UI_FORM_HELPERS_H_

#include <QColor>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace snapper {

// Small shared pieces for the inspector's forms.

// A number field: range, decimals and unit.
void SetUpNumber(QDoubleSpinBox* box, double low, double high, int decimals,
                 const QString& suffix);

// Sets a field without it reporting a change back.
void ShowNumber(QDoubleSpinBox* box, double value);

// Paints a colour button with its colour.
void ShowColour(QPushButton* button, const QColor& colour);

// Asks for a colour starting from current; invalid when cancelled.
QColor AskColour(QWidget* parent, const QColor& current);

// Greys widget out with why_not as its tooltip, or enables it.
void Explain(QWidget* widget, const QString& why_not);

}  // namespace snapper

#endif  // SNAPPER_UI_FORM_HELPERS_H_
