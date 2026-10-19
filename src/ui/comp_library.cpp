#include "ui/comp_library.h"

#include <QApplication>
#include <QDrag>
#include <QSet>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>

#include <cassert>
#include <vector>

#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "fsp/fsp_effects.h"
#include "model/comp.h"
#include "ui/comp_drag.h"
#include "ui/effect_row_delegate.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Item data: the entry's kind, its plugin, whether it is the Scene
// gradient or a header, and the words the search also looks in.
// F's Plugins carries about 50; more is a broken list.
constexpr size_t kMaxPlugins = 512;
constexpr int kMaxItems = 1024;
constexpr char kFoldsKey[] = "comp/folded";

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
  // Where an effect goes is a note, not a setting: quiet, like a hint.
  QPalette quiet = where_.palette();
  quiet.setColor(QPalette::WindowText, theme::kTextOff);
  where_.setPalette(quiet);
  where_.setContentsMargins(0, 2, 0, 4);
  list_.setMouseTracking(true);
  list_.setFrameShape(QFrame::NoFrame);
  list_.setItemDelegate(&delegate_);
  // Rows light under the pointer.
  list_.viewport()->setAttribute(Qt::WA_Hover);
  list_.setUniformItemSizes(false);
  list_.viewport()->installEventFilter(this);
  none_ = EmptyHint(tr("No effect matches."), &body_);
  none_->hide();
  body_layout_.addWidget(&search_);
  body_layout_.addWidget(&where_);
  body_layout_.addWidget(none_);
  body_layout_.addWidget(&list_, 1);
  layout_.addWidget(PlatedSection(tr("Effects"), &body_, this), 1);
  LoadRecent();
  LoadFolds();
  Fill();
  connect(&search_, &QLineEdit::textChanged, this, &CompLibrary::Filter);
  connect(&search_, &QLineEdit::returnPressed, this,
          [this] { AddPicked(list_.currentItem()); });
  connect(&list_, &QListWidget::itemActivated, this,
          &CompLibrary::AddPicked);
  connect(managers_.comp, &CompManager::Changed, this, [this] {
    const bool is_new = managers_.comp->recent_entries() != listed_recent_;
    if (is_new) {
      SaveRecent();
      Refill();
    }
  });
  const auto where = [this] { ShowWhere(); };
  connect(managers_.history, &HistoryManager::Changed, this, where);
  connect(managers_.selection, &SelectionManager::Changed, this, where);
  ShowWhere();
}

void CompLibrary::AddHeader(const QString& name, const QString& help) {
  assert(!name.isEmpty());
  assert(list_.count() < kMaxItems);
  auto* item = MakeOwned<QListWidgetItem>(name, &list_);
  item->setData(kEffectHeaderRole, true);
  item->setData(kEffectFoldedRole, folded_.contains(name));
  item->setToolTip(help + tr(" Click to fold or open."));
  item->setFlags(Qt::NoItemFlags);
}

void CompLibrary::AddItem(const QString& name, const QString& help,
                          const CompEntry& entry, const QString& why_not) {
  assert(!name.isEmpty());
  assert(list_.count() < kMaxItems);
  auto* item = MakeOwned<QListWidgetItem>(name, &list_);
  item->setData(kEffectKindRole, static_cast<int>(entry.kind));
  item->setData(kEffectPluginRole, entry.plugin);
  item->setData(kEffectSceneRole, entry.is_scene_gradient);
  item->setData(kEffectHelpRole, help);
  const bool can_add = why_not.isEmpty();
  item->setToolTip(can_add ? help : why_not);
  item->setFlags(can_add ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         : Qt::NoItemFlags);
}

std::pair<QString, QString> CompLibrary::WordsOf(const CompEntry& entry) {
  assert(static_cast<int>(entry.kind) >= 0);
  assert(static_cast<int>(entry.kind) < kCompKindCount);
  const bool is_scene = entry.is_scene_gradient;
  if (is_scene) {
    return {tr("Scene gradient"),
            tr("A gradient ready in one click: warm at the top to cool at "
               "the bottom, in Overlay. A sunset tint, a dark vignette, a "
               "top-down light.")};
  }
  const bool is_plugin = entry.kind == CompKind::kFsp;
  if (!is_plugin) {
    return {CompManager::KindName(entry.kind),
            CompManager::KindHelp(entry.kind)};
  }
  const FspEffectInfo* info =
      entry.plugin.isEmpty() ? nullptr : FindFspEffect(entry.plugin);
  const bool has_name = info != nullptr && !info->name.isEmpty();
  return {has_name ? info->name : entry.plugin,
          CompManager::PluginHelp(entry.plugin)};
}

void CompLibrary::Fill() {
  assert(list_.count() == 0);
  const std::vector<FspEffectInfo>& plugins = AllFspEffects();
  assert(plugins.size() <= kMaxPlugins);
  listed_recent_ = managers_.comp->recent_entries();
  const bool has_recent = !listed_recent_.empty();
  if (has_recent) {
    AddHeader(tr("Recently used"), tr("The effects you added last."));
  }
  std::vector<CompEntry> entries = listed_recent_;
  const size_t own_at = entries.size();
  for (int i = 0; i < kCompKindCount; ++i) {
    const auto kind = static_cast<CompKind>(i);
    // Plugins are listed by their own names below.
    const bool is_plugin = kind == CompKind::kFsp;
    if (!is_plugin) {
      entries.push_back(CompEntry{kind, QString(), false});
    }
  }
  entries.push_back(CompEntry{CompKind::kGradient, QString(), true});
  const size_t plugins_at = entries.size();
  // Plugins that can be added first; the greyed ones gather at the end.
  for (const bool wants_ready : {true, false}) {
    for (const FspEffectInfo& info : plugins) {
      const bool is_ready = CompManager::WhyNoPlugin(info.id).isEmpty();
      if (is_ready == wants_ready) {
        entries.push_back(CompEntry{CompKind::kFsp, info.id, false});
      }
    }
  }
  for (size_t i = 0; i < entries.size(); ++i) {
    const bool is_own_start = i == own_at;
    const bool is_plugins_start = i == plugins_at && !plugins.empty();
    if (is_own_start) {
      AddHeader(tr("Snapper"), tr("Snapper's own finishing effects."));
    } else if (is_plugins_start) {
      AddHeader(tr("F's Plugins"), CompManager::KindHelp(CompKind::kFsp));
    }
    const CompEntry& entry = entries[i];
    const auto [name, help] = WordsOf(entry);
    const bool is_plugin = entry.kind == CompKind::kFsp;
    AddItem(name.isEmpty() ? tr("Missing plugin") : name, help, entry,
            is_plugin ? CompManager::WhyNoPlugin(entry.plugin) : QString());
  }
  CountGroups();
}

void CompLibrary::CountGroups() {
  assert(list_.count() < kMaxItems);
  QListWidgetItem* header = nullptr;
  int count = 0;
  for (int row = 0; row <= list_.count(); ++row) {
    QListWidgetItem* item = row < list_.count() ? list_.item(row) : nullptr;
    const bool ends_group =
        item == nullptr || item->data(kEffectHeaderRole).toBool();
    if (!ends_group) {
      ++count;
      continue;
    }
    const bool has_header = header != nullptr;
    if (has_header) {
      header->setData(kEffectCountRole, count);
    }
    header = item;
    count = 0;
  }
}

void CompLibrary::LoadFolds() {
  assert(managers_.document != nullptr);
  assert(folded_.isEmpty());
  const QSettings settings(managers_.document->SettingsPath(),
                           QSettings::IniFormat);
  const QStringList names =
      settings.value(QLatin1String(kFoldsKey)).toStringList();
  folded_ = QSet<QString>(names.begin(), names.end());
}

void CompLibrary::ToggleFold(QListWidgetItem* header) {
  assert(header != nullptr);
  assert(header->data(kEffectHeaderRole).toBool());
  const QString name = header->text();
  const bool was_folded = folded_.contains(name);
  if (was_folded) {
    folded_.remove(name);
  } else {
    folded_.insert(name);
  }
  header->setData(kEffectFoldedRole, !was_folded);
  QSettings settings(managers_.document->SettingsPath(),
                     QSettings::IniFormat);
  settings.setValue(QLatin1String(kFoldsKey),
                    QStringList(folded_.begin(), folded_.end()));
  Filter(search_.text());
}

void CompLibrary::Refill() {
  assert(managers_.comp != nullptr);
  assert(list_.count() < kMaxItems);
  pressed_ = nullptr;
  list_.clear();
  Fill();
  Filter(search_.text());
}

void CompLibrary::LoadRecent() {
  assert(managers_.comp != nullptr);
  assert(managers_.document != nullptr);
  const QSettings settings(managers_.document->SettingsPath(),
                           QSettings::IniFormat);
  std::vector<CompEntry> entries;
  for (const QString& saved :
       settings.value(QStringLiteral("comp/recent")).toStringList()) {
    const QStringList parts = saved.split(QLatin1Char(','));
    bool is_number = false;
    const int kind = parts.value(0).toInt(&is_number);
    const bool is_entry = parts.size() == 3 && is_number && kind >= 0 &&
                          kind < kCompKindCount;
    if (is_entry) {
      entries.push_back(CompEntry{static_cast<CompKind>(kind), parts[2],
                                  parts[1] == QStringLiteral("1")});
    }
  }
  managers_.comp->SetRecentEntries(std::move(entries));
}

void CompLibrary::SaveRecent() {
  assert(managers_.comp != nullptr);
  assert(managers_.document != nullptr);
  QStringList saved;
  for (const CompEntry& entry : managers_.comp->recent_entries()) {
    saved.push_back(QStringLiteral("%1,%2,%3")
                        .arg(static_cast<int>(entry.kind))
                        .arg(entry.is_scene_gradient ? 1 : 0)
                        .arg(entry.plugin));
  }
  QSettings settings(managers_.document->SettingsPath(),
                     QSettings::IniFormat);
  settings.setValue(QStringLiteral("comp/recent"), saved);
}

std::optional<CompEntry> CompLibrary::EntryAt(
    const QListWidgetItem* item) const {
  const bool is_entry = item != nullptr && !item->isHidden() &&
                        !item->data(kEffectHeaderRole).toBool() &&
                        item->flags().testFlag(Qt::ItemIsEnabled);
  if (!is_entry) {
    return std::nullopt;
  }
  const int kind = item->data(kEffectKindRole).toInt();
  assert(kind >= 0 && kind < kCompKindCount);
  assert(list_.count() > 0);
  return CompEntry{static_cast<CompKind>(kind),
                   item->data(kEffectPluginRole).toString(),
                   item->data(kEffectSceneRole).toBool()};
}

void CompLibrary::Filter(const QString& text) {
  assert(list_.count() < kMaxItems);
  assert(text.size() < 100000);
  const QString needle = text.trimmed();
  QListWidgetItem* header = nullptr;
  bool has_group_shown = false;
  // A search looks through folded groups too.
  const bool is_searching = !needle.isEmpty();
  bool is_folded = false;
  QListWidgetItem* first = nullptr;
  for (int row = 0; row < list_.count(); ++row) {
    QListWidgetItem* item = list_.item(row);
    const bool is_header = item->data(kEffectHeaderRole).toBool();
    if (is_header) {
      // A header shows while its group has a match.
      const bool has_last = header != nullptr;
      if (has_last) {
        header->setHidden(!has_group_shown);
      }
      header = item;
      has_group_shown = false;
      is_folded = !is_searching && folded_.contains(item->text());
      continue;
    }
    const bool is_match =
        item->text().contains(needle, Qt::CaseInsensitive) ||
        item->data(kEffectHelpRole).toString().contains(needle, Qt::CaseInsensitive);
    item->setHidden(!is_match || is_folded);
    // A folded group keeps its header, so it can be opened again.
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
      shot, managers_.selection->layer());
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
  const auto target = comp->TargetFor(shot, managers_.selection->layer());
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
    QListWidgetItem* under =
        is_press ? list_.itemAt(mouse->position().toPoint()) : nullptr;
    const bool is_fold_click =
        under != nullptr && under->data(kEffectHeaderRole).toBool();
    if (is_fold_click) {
      ToggleFold(under);
      return true;
    }
    if (is_press) {
      pressed_ = under;
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
