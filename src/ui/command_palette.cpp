#include "ui/command_palette.h"

#include <QLabel>
#include <QMenu>
#include <QTimer>

#include <cassert>
#include <memory>

#include "ui/command_search.h"
#include "ui/panel_kit.h"
#include "ui/screen_fit.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The most menus deep a command can sit.
constexpr int kMostDepth = 4;
// The palette's size in ems; FitDialog caps it to the screen.
const QSizeF kPaletteEms(28.0, 20.0);

QString Plain(const QString& text) {
  assert(text.size() < 100000);
  assert(!text.isNull() || text.isEmpty());
  QString plain = text;
  plain.remove('&');
  return plain.simplified();
}

void Collect(const QMenu* menu, const QString& path, int depth,
             std::vector<Command>* commands) {
  assert(commands != nullptr);
  assert(menu != nullptr);
  const bool is_deep = depth > kMostDepth;
  if (is_deep) {
    return;
  }
  for (QAction* action : menu->actions()) {
    const QString name = Plain(action->text());
    const bool is_entry = !action->isSeparator() && !name.isEmpty();
    if (!is_entry) {
      continue;
    }
    const bool is_submenu = action->menu() != nullptr;
    if (is_submenu) {
      Collect(action->menu(), path + name + ": ", depth + 1, commands);
    } else {
      commands->push_back({path + name, action});
    }
  }
}

}  // namespace

std::vector<Command> CommandsOf(const QMenuBar* bar) {
  assert(bar != nullptr);
  std::vector<Command> commands;
  for (const QAction* top : bar->actions()) {
    const bool has_menu = top->menu() != nullptr;
    if (has_menu) {
      Collect(top->menu(), Plain(top->text()) + ": ", 0, &commands);
    }
  }
  assert(commands.size() < 100000);
  return commands;
}

CommandPalette::CommandPalette(QMenuBar* bar, QWidget* parent)
    : QDialog(parent, Qt::Popup | Qt::FramelessWindowHint),
      bar_(bar),
      layout_(this) {
  assert(bar_ != nullptr);
  assert(parent != nullptr);
  setObjectName("command_palette");
  query_.setObjectName("palette_query");
  query_.setPlaceholderText(tr("Find a command..."));
  found_.setObjectName("palette_found");
  why_ = std::make_unique<QLabel>(this).release();
  why_->setObjectName("palette_why");
  why_->setWordWrap(true);
  // The list and why a greyed one cannot run, framed under the query;
  // the section owns its body, the palette the section.
  auto* body = MakeOwned<QWidget>();
  QVBoxLayout* column = StackIn(body);
  column->setSpacing(theme::kRowGap / 2);
  column->addWidget(&found_, 1);
  column->addWidget(why_);
  layout_.setSpacing(theme::kRowGap);
  layout_.addWidget(&query_);
  layout_.addWidget(PlatedSection(tr("Commands"), body, this), 1);
  query_.installEventFilter(this);
  connect(&query_, &QLineEdit::textChanged, this, [this] { Search(); });
  connect(&query_, &QLineEdit::returnPressed, this, [this] { Run(); });
  connect(&found_, &QListWidget::itemActivated, this, [this] { Run(); });
  connect(&found_, &QListWidget::currentRowChanged, this, [this](int row) {
    const bool is_row = row >= 0 && static_cast<size_t>(row) < shown_.size();
    const QAction* action =
        is_row ? shown_[static_cast<size_t>(row)].action : nullptr;
    const bool is_blocked = action != nullptr && !action->isEnabled();
    why_->setText(is_blocked ? action->toolTip() : QString());
  });
}

void CommandPalette::Open() {
  assert(bar_ != nullptr);
  assert(parentWidget() != nullptr);
  commands_ = CommandsOf(bar_);
  query_.clear();
  Search();
  // Sized to the font and screen, high in the window like a search bar.
  FitDialog(this, kPaletteEms);
  const QWidget* window = parentWidget();
  move(window->mapToGlobal(
      QPoint((window->width() - width()) / 2, window->height() / 6)));
  show();
  query_.setFocus();
}

void CommandPalette::Search() {
  assert(commands_.size() < 100000);
  assert(why_ != nullptr);
  std::vector<QString> names;
  for (const Command& command : commands_) {
    names.push_back(command.name);
  }
  const std::vector<size_t> hits = MatchCommands(names, query_.text());
  shown_.clear();
  found_.clear();
  for (const size_t index : hits) {
    const Command& command = commands_[index];
    const QString keys = command.action->shortcut().toString();
    found_.addItem(keys.isEmpty() ? command.name
                                  : command.name + "    [" + keys + "]");
    QListWidgetItem* item = found_.item(found_.count() - 1);
    item->setToolTip(command.action->isEnabled() ? QString()
                                                 : command.action->toolTip());
    const bool is_blocked = !command.action->isEnabled();
    if (is_blocked) {
      item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    shown_.push_back(command);
  }
  found_.setCurrentRow(found_.count() > 0 ? 0 : -1);
}

void CommandPalette::Run() {
  assert(why_ != nullptr);
  assert(shown_.size() <= commands_.size());
  const int row = found_.currentRow();
  const bool is_row = row >= 0 && static_cast<size_t>(row) < shown_.size();
  if (!is_row) {
    return;
  }
  QAction* action = shown_[static_cast<size_t>(row)].action;
  const bool can_run = action->isEnabled();
  if (!can_run) {
    why_->setText(action->toolTip());
    return;
  }
  close();
  // After the box is gone, so the action runs on the window.
  QTimer::singleShot(0, action, [action] { action->trigger(); });
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched != nullptr);
  const bool is_key = watched == &query_ && event->type() == QEvent::KeyPress;
  if (!is_key) {
    return QDialog::eventFilter(watched, event);
  }
  const int key = static_cast<QKeyEvent*>(event)->key();
  const bool is_down = key == Qt::Key_Down;
  const bool is_up = key == Qt::Key_Up;
  const bool is_arrow = is_down || is_up;
  if (is_arrow) {
    const int next = found_.currentRow() + (is_down ? 1 : -1);
    const bool is_in = next >= 0 && next < found_.count();
    if (is_in) {
      found_.setCurrentRow(next);
    }
    return true;
  }
  return QDialog::eventFilter(watched, event);
}

}  // namespace snapper
