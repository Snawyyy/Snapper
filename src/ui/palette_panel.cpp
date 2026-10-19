#include "ui/palette_panel.h"

#include <QApplication>
#include <QClipboard>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMenu>

#include <cassert>

#include "edit/palette_manager.h"
#include "ui/fold_section.h"
#include "ui/full_view.h"
#include "ui/mode_icons.h"
#include "ui/panel_kit.h"
#include "ui/theme.h"

namespace snapper {
PalettePanel::PalettePanel(PaletteManager* palette, MotionManager* motion,
                           QString settings_path, QWidget* window)
    : QWidget(window),
      palette_(palette),
      settings_path_(std::move(settings_path)),
      layout_(this) {
  assert(palette_ != nullptr);
  assert(window != nullptr);
  SetUpFrame();
  // Drawing in Tab's full view still needs the colors.
  setProperty(kKeepInFullView, true);
  layout_.setContentsMargins(8, 6, 8, 8);
  layout_.setSpacing(4);
  BuildTitle();
  BuildSections(motion);
  auto* row = MakeOwned<QHBoxLayout>(&current_row_);
  row->setContentsMargins(0, 0, 0, 0);
  // The eyedropper sits bottom left, next to the color it fills in.
  row->addWidget(&pick_);
  // A square sunken well filled with the current color.
  current_swatch_.setFixedSize(44, 22);
  current_swatch_.setFrameStyle(QFrame::Panel | QFrame::Sunken);
  current_swatch_.setAutoFillBackground(true);
  row->addWidget(&current_swatch_);
  row->addWidget(&current_hex_, 1);
  layout_.addWidget(&current_row_);
  connect(palette_, &PaletteManager::Changed, this, &PalettePanel::Refresh);
  // Draw mode opens and closes the palette through the manager.
  connect(palette_, &PaletteManager::ShownChanged, this,
          &PalettePanel::SetOpen);
  connect(&fold_, &QToolButton::clicked, this,
          [this] { SetFolded(!is_folded_); });
  connect(&close_, &QToolButton::clicked, this, [this] { SetOpen(false); });
  connect(&pick_, &QToolButton::toggled, this, [this](bool is_on) {
    pick_.setIcon(EyedropperIcon(is_on));
  });
  connect(&pick_, &QToolButton::clicked, this, [this](bool is_on) {
    if (is_on) {
      StartEyedropper();
    } else {
      StopEyedropper();
    }
  });
  SetFolded(false);
  LoadRecent();
  hide();
}

void PalettePanel::BuildTitle() {
  assert(layout_.count() == 0);
  assert(title_.layout() == nullptr);
  auto* title_row = MakeOwned<QHBoxLayout>(&title_);
  title_row->setContentsMargins(0, 0, 0, 0);
  title_text_.setText(tr("Palette"));
  QFont bold = title_text_.font();
  bold.setBold(true);
  title_text_.setFont(bold);
  pick_.setAutoRaise(true);
  pick_.setCheckable(true);
  pick_.setIcon(EyedropperIcon(false));
  pick_.setIconSize(QSize(kModeIconPx, kModeIconPx));
  pick_.setFixedSize(26, 22);
  pick_.setToolTip(tr("Eyedropper: click anywhere in the window to pick "
                      "the color there. Escape or a right click cancels."));
  fold_.setAutoRaise(true);
  fold_.setToolTip(tr("Fold to the current color"));
  close_.setAutoRaise(true);
  close_.setText(QStringLiteral("x"));
  close_.setToolTip(tr("Close the palette"));
  title_row->addWidget(&title_text_, 1);
  title_row->addWidget(&fold_);
  title_row->addWidget(&close_);
  layout_.addWidget(&title_);
}

void PalettePanel::BuildSections(MotionManager* motion) {
  assert(motion != nullptr);
  assert(wheel_fold_ == nullptr);
  wheel_fold_ = MakeOwned<FoldSection>(tr("Color wheel"), motion,
                                       settings_path_,
                                       QStringLiteral("palette_wheel"), this,
                                       true);
  wheel_ = MakeOwned<ColorWheel>(wheel_fold_);
  wheel_fold_->form()->addRow(wheel_);
  recent_fold_ = MakeOwned<FoldSection>(tr("Recent"), motion, settings_path_,
                                        QStringLiteral("palette_recent"),
                                        this, true);
  recent_ = MakeOwned<SwatchGrid>(recent_fold_);
  recent_empty_ = EmptyHint(tr("Colors you pick show up here."),
                            recent_fold_);
  recent_fold_->form()->addRow(recent_);
  recent_fold_->form()->addRow(recent_empty_);
  character_fold_ = MakeOwned<FoldSection>(
      tr("Character"), motion, settings_path_,
      QStringLiteral("palette_character"), this, true);
  character_ = MakeOwned<SwatchGrid>(character_fold_);
  character_fold_->form()->addRow(character_);
  for (FoldSection* fold : {wheel_fold_, recent_fold_, character_fold_}) {
    layout_.addWidget(fold);
  }
  connect(wheel_, &ColorWheel::Picked, palette_, &PaletteManager::Pick);
  for (SwatchGrid* grid : {recent_, character_}) {
    connect(grid, &SwatchGrid::Hovered, palette_, &PaletteManager::Hover);
    connect(grid, &SwatchGrid::MenuAsked, this, &PalettePanel::ShowMenu);
  }
}

void PalettePanel::SetOpen(bool is_open) {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  const bool is_same = is_open == isVisible();
  if (is_same) {
    return;
  }
  if (is_open) {
    RestorePlace();
    Refresh();
    show();
    raise();
  } else {
    StopEyedropper();
    KeepPlace();
    hide();
  }
  palette_->SetShown(is_open);
  const bool is_closed = !is_open;
  if (is_closed) {
    KeepRecent();
  }
  emit OpenChanged(is_open);
}

void PalettePanel::Refresh() {
  assert(palette_ != nullptr);
  assert(wheel_ != nullptr);
  const QColor current = palette_->current();
  wheel_->SetColor(current);
  const std::vector<QColor> recent = palette_->RecentColors();
  recent_->SetColors(recent, current);
  recent_empty_->setVisible(recent.empty());
  const bool shows_character = !is_folded_ && palette_->ShowsCharacter();
  character_fold_->setVisible(shows_character);
  character_->SetColors(palette_->CharacterColors(), current);
  character_fold_->SetTitle(
      tr("Character: %1").arg(palette_->CharacterName()));
  ShowCurrent(current);
  FitHeight();
}

void PalettePanel::SetFolded(bool is_folded) {
  assert(wheel_fold_ != nullptr);
  assert(recent_fold_ != nullptr);
  is_folded_ = is_folded;
  fold_.setText(is_folded_ ? QStringLiteral("+") : QStringLiteral("-"));
  wheel_fold_->setVisible(!is_folded_);
  recent_fold_->setVisible(!is_folded_);
  Refresh();
}

void PalettePanel::ShowMenu(const QColor& color, const QPoint& global) {
  assert(palette_ != nullptr);
  assert(width() >= 0);
  const bool has_color = color.isValid();
  if (!has_color) {
    return;
  }
  QMenu menu;
  menu.addAction(tr("Copy hex"), this, [color] {
    QApplication::clipboard()->setText(color.name().toUpper());
  });
  menu.exec(global);
}

QColor PalettePanel::SwatchAt(QPoint at) const {
  assert(recent_ != nullptr);
  assert(character_ != nullptr);
  for (const SwatchGrid* grid : {recent_, character_}) {
    const QPoint local = grid->mapFrom(this, at);
    const bool is_on = grid->isVisible() && grid->rect().contains(local);
    if (is_on) {
      return grid->ColorAt(local);
    }
  }
  return QColor();
}

}  // namespace snapper
