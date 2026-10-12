#include "ui/pose_controls.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>

#include "ui/angle_dial.h"
#include "ui/fader.h"
#include "ui/number_input.h"
#include "ui/scrub_label.h"
#include "ui/track_pad.h"

namespace snapper {
namespace {

constexpr int kReadoutWidth = 62;
// The least a readout in a bar shrinks to.
constexpr int kReadoutLeast = 40;
constexpr int kNameWidth = 60;
// Space between cells that share a row.
constexpr int kCellGap = 16;
// The small dials are 36 px: the least that still shows the snap ticks.
constexpr int kMiniDial = 36;
constexpr int kMiniNameWidth = 56;

// A widget or layout that its parent will own from here on.
template <typename T, typename... Args>
T* Make(Args&&... args) {
  static_assert(std::is_class_v<T>);
  static_assert(sizeof(T) > 0);
  return std::make_unique<T>(std::forward<Args>(args)...).release();
}

// Narrows box to a readout that still takes typed numbers and math.
// A width of 0 lets it grow with its bar.
void MakeReadout(QDoubleSpinBox* box, int width = kReadoutWidth) {
  assert(box != nullptr);
  assert(width >= 0);
  NumberInput::Install(box);
  box->setButtonSymbols(QAbstractSpinBox::NoButtons);
  box->setAlignment(Qt::AlignCenter);
  const bool is_flexible = width == 0;
  if (is_flexible) {
    box->setMinimumWidth(kReadoutLeast);
  } else {
    box->setFixedWidth(width);
  }
}

// A sunken frame round a control, with its readout bar right under it
// (when it has one), both the same width, so the numbers read as part of
// the control.
QWidget* Framed(QWidget* control, QWidget* bar, QWidget* parent) {
  assert(control != nullptr);
  assert(parent != nullptr);
  auto frame = std::make_unique<QFrame>(parent);
  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  auto* column = Make<QVBoxLayout>(frame.get());
  column->setContentsMargins(2, 2, 2, 2);
  column->setSpacing(0);
  control->setParent(frame.get());
  column->addWidget(control);
  const bool has_bar = bar != nullptr;
  if (has_bar) {
    auto* line = Make<QFrame>(frame.get());
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    column->addWidget(line);
    bar->setParent(frame.get());
    column->addWidget(bar);
  }
  return frame.release();
}

// A bar of readouts that share its width, each after its small name.
QWidget* ReadoutBar(const std::vector<std::pair<QString, QDoubleSpinBox*>>&
                        parts,
                    LiveEdit* live, QWidget* parent) {
  assert(!parts.empty());
  assert(parts.size() < 8);
  auto bar = std::make_unique<QWidget>(parent);
  auto* line = Make<QHBoxLayout>(bar.get());
  line->setContentsMargins(4, 2, 4, 2);
  line->setSpacing(4);
  for (const auto& [name, box] : parts) {
    MakeReadout(box, 0);
    const bool is_named = !name.isEmpty();
    if (is_named) {
      line->addWidget(Make<ScrubLabel>(name, box, live));
    }
    line->addWidget(box, 1);
  }
  return bar.release();
}

// Makes label a raised plate, like a header coming out of the panel.
void MakePlate(QLabel* label) {
  assert(label != nullptr);
  assert(!label->text().isEmpty());
  label->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
  label->setAlignment(Qt::AlignCenter);
  label->setMargin(2);
}

// A cell with its parts stacked flush, with no margin of its own: the
// title plate sits right on the frame under it.
QVBoxLayout* StackIn(QWidget* cell) {
  assert(cell != nullptr);
  auto* column = Make<QVBoxLayout>(cell);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  assert(column->parentWidget() == cell);
  return column;
}

// A slot of a fixed height with widget centred in it, so cells that sit
// side by side line up whatever size their control is.
QWidget* InSlot(QWidget* widget, int height, QWidget* parent) {
  assert(widget != nullptr);
  assert(height > 0);
  auto slot = std::make_unique<QWidget>(parent);
  slot->setFixedHeight(height);
  auto* line = Make<QHBoxLayout>(slot.get());
  line->setContentsMargins(0, 0, 0, 0);
  widget->setParent(slot.get());
  line->addWidget(widget, 0, Qt::AlignCenter);
  return slot.release();
}

}  // namespace

QWidget* MakeDialCell(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, int diameter, int slot_height,
                      QWidget* parent) {
  assert(box != nullptr);
  assert(!label.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* column = StackIn(cell.get());
  auto* name = Make<ScrubLabel>(label, box, live);
  MakePlate(name);
  column->addWidget(name);
  QWidget* dial = Make<AngleDial>(box, live, cell.get(), diameter);
  column->addWidget(Framed(InSlot(dial, slot_height, cell.get()),
                           ReadoutBar({{QString(), box}}, live, cell.get()),
                           cell.get()));
  return cell.release();
}

QWidget* MakeSpinCell(const QString& label, QAbstractSpinBox* box,
                      LiveEdit* live, QWidget* parent) {
  assert(box != nullptr);
  assert(!label.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* line = Make<QHBoxLayout>(cell.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(4);
  NumberInput::Install(box);
  box->setButtonSymbols(QAbstractSpinBox::NoButtons);
  box->setAlignment(Qt::AlignCenter);
  auto* name = Make<ScrubLabel>(label, box, live);
  name->setFixedWidth(kMiniNameWidth);
  line->addWidget(name);
  line->addWidget(box, 1);
  return cell.release();
}

QWidget* MakeMiniDial(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, QWidget* parent) {
  assert(box != nullptr);
  assert(!label.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* line = Make<QHBoxLayout>(cell.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(4);
  MakeReadout(box);
  auto* name = Make<ScrubLabel>(label, box, live);
  name->setFixedWidth(kMiniNameWidth);
  line->addWidget(name);
  line->addWidget(Make<AngleDial>(box, live, cell.get(), kMiniDial));
  line->addWidget(box);
  return cell.release();
}

QWidget* MakePlated(const QString& title, QWidget* control, QWidget* parent,
                    QWidget* bar) {
  assert(control != nullptr);
  assert(!title.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* column = StackIn(cell.get());
  auto* plate = Make<QLabel>(title);
  MakePlate(plate);
  column->addWidget(plate);
  column->addWidget(Framed(control, bar, cell.get()));
  return cell.release();
}

QWidget* MakeCellRow(const std::vector<QWidget*>& cells,
                     const std::vector<int>& shares, QWidget* parent) {
  assert(!cells.empty());
  assert(shares.empty() || shares.size() == cells.size());
  auto row = std::make_unique<QWidget>(parent);
  auto* line = Make<QHBoxLayout>(row.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(kCellGap);
  for (size_t i = 0; i < cells.size(); ++i) {
    cells[i]->setParent(row.get());
    line->addWidget(cells[i], shares.empty() ? 1 : shares[i]);
  }
  return row.release();
}

QWidget* MakeFaderRow(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, double shown_high, double neutral,
                      QWidget* parent) {
  assert(box != nullptr);
  assert(!label.isEmpty());
  auto row = std::make_unique<QWidget>(parent);
  auto* line = Make<QHBoxLayout>(row.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(4);
  MakeReadout(box);
  auto* name = Make<ScrubLabel>(label, box, live);
  name->setFixedWidth(kNameWidth);
  line->addWidget(name);
  line->addWidget(
      Make<Fader>(box, live, shown_high, neutral, row.get()), 1);
  line->addWidget(box);
  return row.release();
}

QWidget* MakePadCell(const QString& label, QDoubleSpinBox* x,
                     QDoubleSpinBox* y, LiveEdit* live, QWidget* parent) {
  assert(x != nullptr && y != nullptr);
  assert(!label.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* column = StackIn(cell.get());
  auto* title = Make<QLabel>(label);
  MakePlate(title);
  column->addWidget(title);
  column->addWidget(Framed(
      Make<TrackPad>(x, y, live, cell.get()),
      ReadoutBar({{QStringLiteral("x"), x}, {QStringLiteral("y"), y}}, live,
                 cell.get()),
      cell.get()));
  return cell.release();
}

}  // namespace snapper
