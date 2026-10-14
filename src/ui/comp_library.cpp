#include "ui/comp_library.h"

#include <QApplication>
#include <QDrag>
#include <QKeyEvent>
#include <QMouseEvent>

#include <cassert>
#include <vector>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "fsp/fsp_effects.h"
#include "model/comp.h"
#include "ui/comp_drag.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// Item data: the entry's kind, its plugin, whether it is the Scene
// gradient or a header, and the words the search also looks in.
constexpr int kKindRole = Qt::UserRole;
constexpr int kPluginRole = Qt::UserRole + 1;
constexpr int kSceneRole = Qt::UserRole + 2;
constexpr int kHeaderRole = Qt::UserRole + 3;
constexpr int kHelpRole = Qt::UserRole + 4;
// F's Plugins carries about 50; more is a broken list.
constexpr size_t kMaxPlugins = 512;
constexpr int kMaxItems = 1024;

}  // namespace

CompLibrary::CompLibrary(const Managers& managers)
    : managers_(managers), layout_(this), body_layout_(&body_) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  body_layout_.setContentsMargins(0, 0, 0, 0);
  search_.setPlaceholderText(tr("Search effects (A)"));
  search_.setClearButtonEnabled(true);
  search_.installEventFilter(this);
  where_.setWordWrap(true);
  list_.setMouseTracking(true);
  list_.setFrameShape(QFrame::NoFrame);
  list_.setIconSize(QSize(12, 12));
  list_.viewport()->installEventFilter(this);
  none_ = EmptyHint(tr("No effect matches."), &body_);
  none_->hide();
  body_layout_.addWidget(&search_);
  body_layout_.addWidget(&where_);
  body_layout_.addWidget(none_);
  body_layout_.addWidget(&list_, 1);
  layout_.addWidget(PlatedSection(tr("Effects"), &body_, this), 1);
  Fill();
  connect(&search_, &QLineEdit::textChanged, this, &CompLibrary::Filter);
  connect(&search_, &QLineEdit::returnPressed, this,
          [this] { AddPicked(list_.currentItem()); });
  connect(&list_, &QListWidget::itemActivated, this,
          &CompLibrary::AddPicked);
  const auto where = [this] { ShowWhere(); };
  connect(managers_.history, &HistoryManager::Changed, this, where);
  connect(managers_.selection, &SelectionManager::Changed, this, where);
  ShowWhere();
}

void CompLibrary::AddHeader(const QString& name, const QString& help) {
  assert(!name.isEmpty());
  assert(list_.count() < kMaxItems);
  auto* item = MakeOwned<QListWidgetItem>(name, &list_);
  item->setData(kHeaderRole, true);
  item->setToolTip(help);
  QFont bold = item->font();
  bold.setBold(true);
  item->setFont(bold);
  item->setFlags(Qt::NoItemFlags);
}

void CompLibrary::AddItem(const QString& name, const QString& help,
                          const CompEntry& entry, const QString& why_not) {
  assert(!name.isEmpty());
  assert(list_.count() < kMaxItems);
  auto* item = MakeOwned<QListWidgetItem>(MarkIcon(entry.kind), name, &list_);
  item->setData(kKindRole, static_cast<int>(entry.kind));
  item->setData(kPluginRole, entry.plugin);
  item->setData(kSceneRole, entry.is_scene_gradient);
  item->setData(kHelpRole, help);
  const bool can_add = why_not.isEmpty();
  item->setToolTip(can_add ? help : why_not);
  item->setFlags(can_add ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         : Qt::NoItemFlags);
}

void CompLibrary::Fill() {
  assert(list_.count() == 0);
  const std::vector<FspEffectInfo>& plugins = AllFspEffects();
  assert(plugins.size() <= kMaxPlugins);
  AddHeader(tr("Snapper"), tr("Snapper's own finishing effects."));
  for (int i = 0; i < kCompKindCount; ++i) {
    const auto kind = static_cast<CompKind>(i);
    // Plugins are listed by their own names below.
    const bool is_plugin = kind == CompKind::kFsp;
    if (!is_plugin) {
      AddItem(CompManager::KindName(kind), CompManager::KindHelp(kind),
              CompEntry{kind, QString(), false}, QString());
    }
  }
  AddItem(tr("Scene gradient"),
          tr("A gradient ready in one click: warm at the top to cool at "
             "the bottom, in Overlay. A sunset tint, a dark vignette, a "
             "top-down light."),
          CompEntry{CompKind::kGradient, QString(), true}, QString());
  const bool has_plugins = !plugins.empty();
  if (has_plugins) {
    AddHeader(tr("F's Plugins"), CompManager::KindHelp(CompKind::kFsp));
  }
  for (const FspEffectInfo& info : plugins) {
    AddItem(info.name.isEmpty() ? info.id : info.name,
            CompManager::PluginHelp(info.id),
            CompEntry{CompKind::kFsp, info.id, false},
            CompManager::WhyNoPlugin(info.id));
  }
}

std::optional<CompEntry> CompLibrary::EntryAt(
    const QListWidgetItem* item) const {
  const bool is_entry = item != nullptr && !item->isHidden() &&
                        !item->data(kHeaderRole).toBool() &&
                        item->flags().testFlag(Qt::ItemIsEnabled);
  if (!is_entry) {
    return std::nullopt;
  }
  const int kind = item->data(kKindRole).toInt();
  assert(kind >= 0 && kind < kCompKindCount);
  assert(list_.count() > 0);
  return CompEntry{static_cast<CompKind>(kind),
                   item->data(kPluginRole).toString(),
                   item->data(kSceneRole).toBool()};
}

void CompLibrary::Filter(const QString& text) {
  assert(list_.count() < kMaxItems);
  assert(text.size() < 100000);
  const QString needle = text.trimmed();
  QListWidgetItem* header = nullptr;
  bool has_group_shown = false;
  QListWidgetItem* first = nullptr;
  for (int row = 0; row < list_.count(); ++row) {
    QListWidgetItem* item = list_.item(row);
    const bool is_header = item->data(kHeaderRole).toBool();
    if (is_header) {
      // A header shows while its group has a match.
      const bool has_last = header != nullptr;
      if (has_last) {
        header->setHidden(!has_group_shown);
      }
      header = item;
      has_group_shown = false;
      continue;
    }
    const bool is_match =
        item->text().contains(needle, Qt::CaseInsensitive) ||
        item->data(kHelpRole).toString().contains(needle, Qt::CaseInsensitive);
    item->setHidden(!is_match);
    has_group_shown = has_group_shown || is_match;
    const bool is_first = first == nullptr && EntryAt(item).has_value();
    first = is_first ? item : first;
  }
  const bool has_last = header != nullptr;
  if (has_last) {
    header->setHidden(!has_group_shown);
  }
  none_->setVisible(first == nullptr && !needle.isEmpty());
  // Enter in the search adds the best match.
  list_.setCurrentItem(first);
}

void CompLibrary::ShowWhere() {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const auto target = managers_.comp->TargetFor(
      shot, managers_.selection->layer(), managers_.selection->pieces());
  const QString why_full = managers_.comp->WhyNoAdd(shot);
  const QString why_not = !why_full.isEmpty() ? why_full
                          : !target           ? target.error().message
                                              : QString();
  where_.setText(why_not.isEmpty()
                     ? tr("Adds to %1. Click the stage to pick something "
                          "else.")
                           .arg(managers_.comp->TargetText(shot, *target))
                     : why_not);
  where_.setEnabled(why_not.isEmpty());
}

void CompLibrary::AddPicked(const QListWidgetItem* item) {
  const auto entry = EntryAt(item);
  const bool is_entry = entry.has_value();
  if (!is_entry) {
    return;
  }
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  CompManager* comp = managers_.comp;
  const ShotId shot = managers_.selection->shot();
  const auto target = comp->TargetFor(shot, managers_.selection->layer(),
                                      managers_.selection->pieces());
  if (!target) {
    Report(target.error().message);
    return;
  }
  Report(ProblemOf(comp->AddEntry(shot, *entry, *target, kOnTop)));
}

void CompLibrary::StartDrag(const QListWidgetItem* item) {
  const auto entry = EntryAt(item);
  const bool is_entry = entry.has_value();
  if (!is_entry) {
    return;
  }
  assert(item != nullptr);
  assert(list_.viewport() != nullptr);
  auto* drag = MakeOwned<QDrag>(this);
  drag->setMimeData(EntryMime(*entry).release());
  drag->setPixmap(list_.viewport()->grab(list_.visualItemRect(item)));
  drag->exec(Qt::CopyAction);
}

void CompLibrary::FocusSearch() {
  assert(search_.parentWidget() != nullptr);
  assert(list_.count() > 0);
  search_.setFocus(Qt::ShortcutFocusReason);
  search_.selectAll();
}

bool CompLibrary::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched != nullptr);
  const QEvent::Type type = event->type();
  const bool is_list = watched == list_.viewport();
  if (is_list) {
    const auto* mouse = static_cast<const QMouseEvent*>(event);
    const bool is_press = type == QEvent::MouseButtonPress &&
                          mouse->button() == Qt::LeftButton;
    const bool is_drag =
        type == QEvent::MouseMove && pressed_ != nullptr &&
        mouse->buttons().testFlag(Qt::LeftButton) &&
        (mouse->position().toPoint() - press_at_).manhattanLength() >=
            QApplication::startDragDistance();
    if (is_press) {
      pressed_ = list_.itemAt(mouse->position().toPoint());
      press_at_ = mouse->position().toPoint();
    } else if (is_drag) {
      const QListWidgetItem* item = pressed_;
      pressed_ = nullptr;
      StartDrag(item);
      return true;
    }
    return QWidget::eventFilter(watched, event);
  }
  const bool is_search_key = watched == &search_ && type == QEvent::KeyPress;
  if (!is_search_key) {
    return QWidget::eventFilter(watched, event);
  }
  // Up and Down walk the list while typing; Escape clears and leaves.
  const auto* key = static_cast<const QKeyEvent*>(event);
  const bool is_walk = key->key() == Qt::Key_Up || key->key() == Qt::Key_Down;
  if (is_walk) {
    QApplication::sendEvent(&list_, event);
    return true;
  }
  const bool is_escape = key->key() == Qt::Key_Escape;
  if (is_escape) {
    search_.clear();
    list_.setFocus();
    return true;
  }
  return QWidget::eventFilter(watched, event);
}

}  // namespace snapper
