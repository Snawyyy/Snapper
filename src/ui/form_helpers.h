#ifndef SNAPPER_UI_FORM_HELPERS_H_
#define SNAPPER_UI_FORM_HELPERS_H_

#include <QColor>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include "base/frame.h"

namespace snapper {

// Small shared pieces for the inspector's forms.

// The kinds of number the panels show. Each has one range, step and
// unit, so the same kind feels the same everywhere.
enum class Number {
  kPixels,    // Positions: any size, step 1 px.
  kSize,      // Sizes and widths: 0 up, step 1 px.
  kDegrees,   // Turns: step 1 degree.
  kLean,      // Skew: -85 to 85 degrees.
  kScale,     // Scale: 0 to 10, step 0.05.
  kFraction,  // Opacity, strength: 0 to 1, step 0.05.
  kZoom,      // Camera zoom: 0.05 to 10, step 0.05.
};

// Sets box up as kind; prefix labels one half of a pair ("x ", "y ").
void SetUpNumber(QDoubleSpinBox* box, Number kind,
                 const QString& prefix = QString());

// A form title above a group of rows, such as "Move" or "Warp".
void AddTitle(QFormLayout* form, QLabel* title, const QString& text);

// Two fields side by side on one form row, such as x and y.
void AddPair(QFormLayout* form, const QString& label, QHBoxLayout* row,
             QWidget* first, QWidget* second);

// Sets a field without it reporting a change back.
void ShowNumber(QDoubleSpinBox* box, double value);
void ShowNumber(QSpinBox* box, int value);

// Paints a colour button with its colour.
void ShowColour(QPushButton* button, const QColor& colour);

// Asks for a colour starting from current; invalid when cancelled.
QColor AskColour(QWidget* parent, const QColor& current);

// frame as minutes and seconds, such as 1:05, as a ruler shows it.
QString TimeText(Frame frame);

// Greys widget out with why_not as its tooltip, or enables it.
void Explain(QWidget* widget, const QString& why_not);
// The same, but an enabled widget keeps help as its tooltip.
void Explain(QWidget* widget, const QString& why_not, const QString& help);

}  // namespace snapper

#endif  // SNAPPER_UI_FORM_HELPERS_H_
