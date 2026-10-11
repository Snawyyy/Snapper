#ifndef SNAPPER_UI_FORM_HELPERS_H_
#define SNAPPER_UI_FORM_HELPERS_H_

#include <QAbstractSpinBox>
#include <QAction>
#include <QColor>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QWidget>

#include <vector>

#include "base/frame.h"

namespace snapper {

class LiveEdit;

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
  kCells,     // Warp reach: 0 to kMaxWarpCells cells, step 0.5.
};

// Sets box up as kind; prefix labels one half of a pair ("x ", "y ").
void SetUpNumber(QDoubleSpinBox* box, Number kind,
                 const QString& prefix = QString());

// How much one pixel of dragging a field's label changes it (see
// ScrubLabel). SetUpNumber sets it by kind; the default is 1.
void SetScrubUnit(QAbstractSpinBox* box, double unit);

// A number field with the name beside it that scrubs it, and math
// typed into it ("*2", "90/2"). live is the field's live edit, so a
// drag is one undo step; null for a field that edits no project.
void AddNumberRow(QFormLayout* form, const QString& label,
                  QAbstractSpinBox* box, LiveEdit* live);

// The same for an angle: a small dial sits beside the field, showing
// the angle and turning it (see AngleDial).
void AddAngleRow(QFormLayout* form, const QString& label,
                 QDoubleSpinBox* box, LiveEdit* live);

// One half of a pair: a short scrubbing name and its field.
struct NumberPart final {
  QString text;
  QAbstractSpinBox* box;
};

// Two number fields side by side on one form row, such as x and y,
// each with its own scrubbing name. Their prefixes must be empty.
void AddNumberPair(QFormLayout* form, const QString& label, QHBoxLayout* row,
                   const NumberPart& first, const NumberPart& second,
                   LiveEdit* live);

// A form title above a group of rows, such as "Move" or "Warp".
void AddTitle(QFormLayout* form, QLabel* title, const QString& text);

// Two fields side by side on one form row, such as x and y.
void AddPair(QFormLayout* form, const QString& label, QHBoxLayout* row,
             QWidget* first, QWidget* second);

// Sets a field without it reporting a change back.
void ShowNumber(QDoubleSpinBox* box, double value);
void ShowNumber(QSpinBox* box, int value);

// Shows "Mixed" in a field instead of its number, unless it is being
// typed in. The next ShowNumber puts the number back.
void ShowMixed(QAbstractSpinBox* box);

// The same for picked things: when their values differ the field says
// "Mixed", keeping focus_value underneath for a drag to start from.
void ShowNumber(QDoubleSpinBox* box, double focus_value,
                const std::vector<double>& picked);
void ShowNumber(QSpinBox* box, int focus_value,
                const std::vector<double>& picked);

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
// The same for a menu entry.
void Explain(QAction* action, const QString& why_not);

}  // namespace snapper

#endif  // SNAPPER_UI_FORM_HELPERS_H_
