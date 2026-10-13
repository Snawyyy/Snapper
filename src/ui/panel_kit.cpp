#include "ui/panel_kit.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPalette>

#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper {
namespace {

// The margin inside a frame, and above and below a title plate's text.
constexpr int kFrameMargin = 2;
constexpr int kPlateMargin = 2;
// Room either side of a title plate's text.
constexpr int kTitlePadX = 6;
// Margin inside a section's frame, past the indent on the left.
constexpr int kSectionMargin = 4;

}  // namespace

QWidget* SharedRow(const std::vector<QWidget*>& cells,
                   const std::vector<int>& shares, int gap,
                   QWidget* parent) {
  assert(!cells.empty());
  assert(shares.empty() || shares.size() == cells.size());
  auto row = std::make_unique<QWidget>(parent);
  auto* line = MakeOwned<QHBoxLayout>(row.get());
  line->setContentsMargins(0, 0, 0, 0);
  line->setSpacing(gap);
  for (size_t i = 0; i < cells.size(); ++i) {
    assert(cells[i] != nullptr);
    cells[i]->setParent(row.get());
    line->addWidget(cells[i], shares.empty() ? 1 : shares[i]);
  }
  return row.release();
}

GoldenParts GoldenSplit(int total) {
  assert(total >= 0);
  assert(kGoldenRatio > 1.0);
  GoldenParts parts;
  parts.long_part = static_cast<int>(std::lround(total / kGoldenRatio));
  parts.short_part = total - parts.long_part;
  assert(parts.long_part >= parts.short_part);
  return parts;
}

void MakePlate(QLabel* label) {
  assert(label != nullptr);
  assert(!label->text().isEmpty());
  label->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
  label->setAlignment(Qt::AlignCenter);
  label->setMargin(kPlateMargin);
}

void MakeTitlePlate(QLabel* label) {
  assert(label != nullptr);
  assert(kTitlePadX > 0);
  label->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label->setMargin(kPlateMargin);
  label->setIndent(kTitlePadX);
  QFont bold = label->font();
  bold.setBold(true);
  label->setFont(bold);
  // Spans the whole width, so it reads as a title bar, not a button.
  label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QWidget* Framed(QWidget* control, QWidget* bar, QWidget* parent) {
  assert(control != nullptr);
  assert(parent != nullptr);
  auto frame = std::make_unique<QFrame>(parent);
  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  auto* column = MakeOwned<QVBoxLayout>(frame.get());
  column->setContentsMargins(kFrameMargin, kFrameMargin, kFrameMargin,
                             kFrameMargin);
  column->setSpacing(0);
  control->setParent(frame.get());
  column->addWidget(control);
  const bool has_bar = bar != nullptr;
  if (has_bar) {
    auto* line = MakeOwned<QFrame>(frame.get());
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    column->addWidget(line);
    bar->setParent(frame.get());
    column->addWidget(bar);
  }
  return frame.release();
}

QVBoxLayout* StackIn(QWidget* cell) {
  assert(cell != nullptr);
  assert(cell->layout() == nullptr);
  auto* column = MakeOwned<QVBoxLayout>(cell);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(0);
  return column;
}

QWidget* InSlot(QWidget* widget, int height, QWidget* parent) {
  assert(widget != nullptr);
  assert(height > 0);
  auto slot = std::make_unique<QWidget>(parent);
  slot->setFixedHeight(height);
  auto* line = MakeOwned<QHBoxLayout>(slot.get());
  line->setContentsMargins(0, 0, 0, 0);
  widget->setParent(slot.get());
  line->addWidget(widget, 0, Qt::AlignCenter);
  return slot.release();
}

QWidget* PlatedSection(const QString& title, QWidget* body, QWidget* parent) {
  assert(body != nullptr);
  assert(!title.isEmpty());
  auto section = std::make_unique<QWidget>(parent);
  auto* column = StackIn(section.get());
  auto* plate = MakeOwned<QLabel>(title, section.get());
  plate->setObjectName(QStringLiteral("plate"));
  MakeTitlePlate(plate);
  column->addWidget(plate);
  auto* frame = MakeOwned<QFrame>(section.get());
  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  auto* inside = MakeOwned<QVBoxLayout>(frame);
  inside->setContentsMargins(theme::kIndent, kSectionMargin, kSectionMargin,
                             kSectionMargin);
  body->setParent(frame);
  inside->addWidget(body);
  column->addWidget(frame);
  return section.release();
}

QFormLayout* SettingsForm(QWidget* host) {
  assert(host != nullptr);
  assert(host->layout() == nullptr);
  auto* form = MakeOwned<QFormLayout>(host);
  form->setContentsMargins(0, 0, 0, 0);
  form->setVerticalSpacing(theme::kRowGap / 2);
  form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
  return form;
}

SettingsGroup::SettingsGroup(const QString& title, QWidget* parent)
    : QWidget(parent), layout_(this), title_(title), form_(&body_) {
  assert(!title.isEmpty());
  assert(theme::kIndent > 0);
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(theme::kRowGap / 2);
  MakeTitlePlate(&title_);
  layout_.addWidget(&title_);
  layout_.addWidget(&body_);
  form_.setContentsMargins(theme::kIndent, 0, 0, 0);
  form_.setVerticalSpacing(theme::kRowGap / 2);
  form_.setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  form_.setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
}

void SettingsGroup::SetTitle(const QString& title) {
  assert(!title.isEmpty());
  assert(title.size() < 1000);
  title_.setText(title);
}

QWidget* GoldenRow(QWidget* long_cell, QWidget* short_cell, QWidget* parent) {
  assert(long_cell != nullptr && short_cell != nullptr);
  assert(long_cell != short_cell);
  return SharedRow({long_cell, short_cell}, {kGoldenLong, kGoldenShort},
                   theme::kCellGap, parent);
}

QWidget* TileRow(const std::vector<QWidget*>& tiles, QWidget* parent) {
  assert(!tiles.empty());
  assert(tiles.size() < 100);
  return SharedRow(tiles, {}, kTileGap, parent);
}

QLabel* EmptyHint(const QString& text, QWidget* parent) {
  assert(!text.isEmpty());
  assert(theme::kTextOff.isValid());
  auto hint = std::make_unique<QLabel>(text, parent);
  hint->setObjectName(QStringLiteral("empty_hint"));
  hint->setWordWrap(true);
  hint->setAlignment(Qt::AlignCenter);
  hint->setMargin(theme::kRowGap);
  QPalette quiet = hint->palette();
  quiet.setColor(QPalette::WindowText, theme::kTextOff);
  hint->setPalette(quiet);
  return hint.release();
}

}  // namespace snapper
