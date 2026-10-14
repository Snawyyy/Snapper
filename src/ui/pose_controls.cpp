#include "ui/pose_controls.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <cassert>
#include <memory>
#include <utility>

#include "ui/angle_dial.h"
#include "ui/fader.h"
#include "ui/number_input.h"
#include "ui/panel_kit.h"
#include "ui/scrub_label.h"
#include "ui/theme.h"
#include "ui/track_pad.h"

namespace snapper {
namespace {

constexpr int kReadoutWidth = 62;
// The least a readout in a bar shrinks to.
constexpr int kReadoutLeast = 40;
// The small dials are 36 px: the least that still shows the snap ticks.
constexpr int kMiniDial = 36;
constexpr int kMiniNameWidth = 56;

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

// A bar of readouts that share its width, each after its small name.
QWidget* ReadoutBar(const std::vector<std::pair<QString, QDoubleSpinBox*>>&
                        parts,
                    LiveEdit* live, QWidget* parent) {
  assert(!parts.empty());
  assert(parts.size() < 8);
  auto bar = std::make_unique<QWidget>(parent);
  auto* line = MakeOwned<QHBoxLayout>(bar.get());
  line->setContentsMargins(4, 2, 4, 2);
  line->setSpacing(4);
  for (const auto& [name, box] : parts) {
    MakeReadout(box, 0);
    const bool is_named = !name.isEmpty();
    if (is_named) {
      line->addWidget(MakeOwned<ScrubLabel>(name, box, live));
    }
    line->addWidget(box, 1);
  }
  return bar.release();
}

}  // namespace

QWidget* MakeDialCell(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, int diameter, int slot_height,
                      QWidget* parent) {
  assert(box != nullptr);
  assert(!label.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* column = StackIn(cell.get());
  auto* name = MakeOwned<ScrubLabel>(label, box, live);
  MakePlate(name);
  column->addWidget(name);
  QWidget* dial = MakeOwned<AngleDial>(box, live, cell.get(), diameter);
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
  auto* line = MakeOwned<QHBoxLayout>(cell.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(4);
  NumberInput::Install(box);
  box->setButtonSymbols(QAbstractSpinBox::NoButtons);
  box->setAlignment(Qt::AlignCenter);
  auto* name = MakeOwned<ScrubLabel>(label, box, live);
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
  auto* line = MakeOwned<QHBoxLayout>(cell.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(4);
  MakeReadout(box);
  auto* name = MakeOwned<ScrubLabel>(label, box, live);
  name->setFixedWidth(kMiniNameWidth);
  line->addWidget(name);
  line->addWidget(MakeOwned<AngleDial>(box, live, cell.get(), kMiniDial));
  line->addWidget(box);
  return cell.release();
}

QWidget* MakePlated(const QString& title, QWidget* control, QWidget* parent,
                    QWidget* bar) {
  assert(control != nullptr);
  assert(!title.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* column = StackIn(cell.get());
  auto* plate = MakeOwned<QLabel>(title);
  MakePlate(plate);
  column->addWidget(plate);
  column->addWidget(Framed(control, bar, cell.get()));
  return cell.release();
}

QWidget* MakeCellRow(const std::vector<QWidget*>& cells,
                     const std::vector<int>& shares, QWidget* parent) {
  assert(!cells.empty());
  assert(shares.empty() || shares.size() == cells.size());
  return SharedRow(cells, shares, theme::kCellGap, parent);
}

QWidget* MakeNamedRow(const QString& label, QWidget* field,
                      QWidget* parent) {
  assert(field != nullptr);
  assert(!label.isEmpty());
  auto row = std::make_unique<QWidget>(parent);
  auto* line = MakeOwned<QHBoxLayout>(row.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(4);
  auto* name = MakeOwned<QLabel>(label, row.get());
  name->setFixedWidth(kFaderNameWidth);
  line->addWidget(name);
  line->addWidget(field, 1);
  return row.release();
}

QWidget* MakeFaderRow(const QString& label, QDoubleSpinBox* box,
                      LiveEdit* live, double shown_high, double neutral,
                      QWidget* parent) {
  assert(box != nullptr);
  assert(!label.isEmpty());
  auto row = std::make_unique<QWidget>(parent);
  auto* line = MakeOwned<QHBoxLayout>(row.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(4);
  MakeReadout(box);
  auto* name = MakeOwned<ScrubLabel>(label, box, live);
  name->setFixedWidth(kFaderNameWidth);
  line->addWidget(name);
  line->addWidget(
      MakeOwned<Fader>(box, live, shown_high, neutral, row.get()), 1);
  line->addWidget(box);
  return row.release();
}

QWidget* MakePadCell(const QString& label, QDoubleSpinBox* x,
                     QDoubleSpinBox* y, LiveEdit* live, QWidget* parent) {
  assert(x != nullptr && y != nullptr);
  assert(!label.isEmpty());
  auto cell = std::make_unique<QWidget>(parent);
  auto* column = StackIn(cell.get());
  auto* title = MakeOwned<QLabel>(label);
  MakePlate(title);
  column->addWidget(title);
  column->addWidget(Framed(
      MakeOwned<TrackPad>(x, y, live, cell.get()),
      ReadoutBar({{QStringLiteral("x"), x}, {QStringLiteral("y"), y}}, live,
                 cell.get()),
      cell.get()));
  return cell.release();
}

}  // namespace snapper
