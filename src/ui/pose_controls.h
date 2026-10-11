#ifndef SNAPPER_UI_POSE_CONTROLS_H_
#define SNAPPER_UI_POSE_CONTROLS_H_

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QString>
#include <QWidget>

#include <vector>

namespace snapper {

class LiveEdit;

// The hand-made controls of the pose panel, each wrapped with its name
// and a small number readout (the field itself, narrowed, so a value
// can still be typed or scrubbed). Every cell works the fields it is
// given; the parent owns the cell.

// A dial with its name above and the readout below. The dial sits
// centred in a slot slot_height tall, so cells side by side line up.
QWidget* MakeDialCell(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, int diameter, int slot_height,
                      QWidget* parent);
// A name and a whole-number field that grows with its cell (for what
// has no analog control, such as a cycle in frames).
QWidget* MakeSpinCell(const QString& label, QAbstractSpinBox* box,
                      LiveEdit* live, QWidget* parent);
// A small dial in a row: name, dial, readout.
QWidget* MakeMiniDial(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, QWidget* parent);
// A raised title plate over a sunken frame holding control (and, under
// it, bar when given): how a group of settings is bordered together.
QWidget* MakePlated(const QString& title, QWidget* control, QWidget* parent,
                    QWidget* bar = nullptr);
// Lays cells side by side, sharing the width by shares (one per cell, or
// empty for equal shares).
QWidget* MakeCellRow(const std::vector<QWidget*>& cells,
                     const std::vector<int>& shares, QWidget* parent);
// Name, fader and readout in a row. The groove spans the field's
// minimum to shown_high; neutral is where double click returns.
QWidget* MakeFaderRow(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, double shown_high, double neutral,
                      QWidget* parent);
// A pad that moves x and y, titled above, with their readouts under it.
QWidget* MakePadCell(const QString& label, QDoubleSpinBox* x,
                     QDoubleSpinBox* y, LiveEdit* live, QWidget* parent);

}  // namespace snapper

#endif  // SNAPPER_UI_POSE_CONTROLS_H_
