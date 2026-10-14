#include "ui/comp_library.h"

#include <QApplication>
#include <QKeyEvent>

#include <cassert>
#include <vector>

#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "fsp/fsp_effects.h"
#include "model/comp.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// What an entry adds: a CompKind, or one of these.
constexpr int kSceneGradient = -1;
constexpr int kHeader = -2;
constexpr int kKindRole = Qt::UserRole;
constexpr int kPluginRole = Qt::UserRole + 1;
constexpr int kHelpRole = Qt::UserRole + 2;
// F's Plugins carries about 50; more is a broken list.
constexpr size_t kMaxPlugins = 512;

}  // namespace

CompLibrary::CompLibrary(const Managers& managers)
    : managers_(managers), layout_(this), body_layout_(&body_) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  body_layout_.setContentsMargins(0, 0, 0, 0);
  search_.setPlaceholderText(tr("Search effects"));
  search_.setClearButtonEnabled(true);
  search_.installEventFilter(this);
  where_.setWordWrap(true);
  list_.setMouseTracking(true);
  list_.setFrameShape(QFrame::NoFrame);
  body_layout_.addWidget(&search_);
  body_layout_.addWidget(&where_);
  body_layout_.addWidget(&list_, 1);
  layout_.addWidget(PlatedSection(tr("Effects"), &body_, this), 1);
  Fill();
  connect(&search_, &QLineEdit::textChanged, this, &CompLibrary::Filter);
  connect(&search_, &QLineEdit::returnPressed, this, [this] {
    AddPicked(list_.currentItem());
  });
  connect(&list_, &QListWidget::itemActivated, this,
          &CompLibrary::AddPicked);
  const auto where = [this] { ShowWhere(); };
  connect(managers_.history, &HistoryManager::Changed, this, where);
  connect(managers_.selection, &SelectionManager::Changed, this, where);
  ShowWhere();
}

void CompLibrary::AddEntry(const QString& name, const QString& help,
                           int kind, const QString& plugin,
                           const QString& why_not) {
  assert(!name.isEmpty());
  assert(kind >= kHeader && kind < kCompKindCount);
  auto* item = MakeOwned<QListWidgetItem>(name, &list_);
  item->setData(kKindRole, kind);
  item->setData(kPluginRole, plugin);
  item->setData(kHelpRole, help);
  const bool is_header = kind == kHeader;
  const bool can_add = why_not.isEmpty() && !is_header;
  item->setToolTip(why_not.isEmpty() ? help : why_not);
  if (is_header) {
    QFont bold = item->font();
    bold.setBold(true);
    item->setFont(bold);
    item->setFlags(Qt::NoItemFlags);
    return;
  }
  item->setFlags(can_add ? Qt::ItemIsEnabled | Qt::ItemIsSelectable
                         : Qt::NoItemFlags);
}

void CompLibrary::Fill() {
  assert(list_.count() == 0);
  const std::vector<FspEffectInfo>& plugins = AllFspEffects();
  assert(plugins.size() <= kMaxPlugins);
  for (int i = 0; i < kCompKindCount; ++i) {
    const auto kind = static_cast<CompKind>(i);
    // Plugins are listed by their own names below.
    const bool is_plugin = kind == CompKind::kFsp;
    if (!is_plugin) {
      AddEntry(CompManager::KindName(kind), CompManager::KindHelp(kind), i,
               QString(), QString());
    }
  }
  AddEntry(tr("Scene gradient"),
           tr("A gradient over the whole shot in one click: a sunset "
              "tint, a dark vignette, a top-down light. Always on the "
              "screen."),
           kSceneGradient, QString(), QString());
  const bool has_plugins = !plugins.empty();
  if (has_plugins) {
    AddEntry(tr("F's Plugins"), CompManager::KindHelp(CompKind::kFsp),
             kHeader, QString(), QString());
  }
  const int fsp = static_cast<int>(CompKind::kFsp);
  for (const FspEffectInfo& info : plugins) {
    AddEntry(info.name.isEmpty() ? info.id : info.name,
             CompManager::PluginHelp(info.id),
             fsp, info.id, CompManager::WhyNoPlugin(info.id));
  }
}

void CompLibrary::Filter(const QString& text) {
  assert(list_.count() >= 0);
  assert(text.size() < 100000);
  const QString needle = text.trimmed();
  QListWidgetItem* header = nullptr;
  bool has_plugin_shown = false;
  QListWidgetItem* first = nullptr;
  for (int row = 0; row < list_.count(); ++row) {
    QListWidgetItem* item = list_.item(row);
    const bool is_header = item->data(kKindRole).toInt() == kHeader;
    if (is_header) {
      header = item;
      continue;
    }
    const bool is_match =
        item->text().contains(needle, Qt::CaseInsensitive) ||
        item->data(kHelpRole).toString().contains(needle, Qt::CaseInsensitive);
    item->setHidden(!is_match);
    const bool is_plugin = item->data(kKindRole).toInt() ==
                           static_cast<int>(CompKind::kFsp);
    has_plugin_shown = has_plugin_shown || (is_match && is_plugin);
    const bool is_first = is_match && first == nullptr &&
                          item->flags().testFlag(Qt::ItemIsEnabled);
    if (is_first) {
      first = item;
    }
  }
  const bool has_header = header != nullptr;
  if (has_header) {
    header->setHidden(!has_plugin_shown);
  }
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
                     ? tr("Double click adds to: %1")
                           .arg(managers_.comp->TargetText(shot, *target))
                     : why_not);
  where_.setEnabled(why_not.isEmpty());
}

void CompLibrary::AddPicked(const QListWidgetItem* item) {
  const bool is_entry = item != nullptr && !item->isHidden() &&
                        item->flags().testFlag(Qt::ItemIsEnabled);
  if (!is_entry) {
    return;
  }
  const int kind = item->data(kKindRole).toInt();
  assert(kind >= kHeader && kind < kCompKindCount);
  assert(managers_.comp != nullptr);
  CompManager* comp = managers_.comp;
  const ShotId shot = managers_.selection->shot();
  const bool is_scene = kind == kSceneGradient;
  if (is_scene) {
    const auto added = comp->AddSceneGradient(shot);
    Report(ProblemOf(added));
    // It is on the screen: show the screen's list, where it went.
    const bool is_added = added.has_value();
    if (is_added) {
      managers_.selection->Clear();
    }
    return;
  }
  const auto target = comp->TargetFor(shot, managers_.selection->layer(),
                                      managers_.selection->pieces());
  if (!target) {
    Report(target.error().message);
    return;
  }
  const bool is_plugin = kind == static_cast<int>(CompKind::kFsp);
  Report(ProblemOf(
      is_plugin
          ? comp->AddPlugin(shot, item->data(kPluginRole).toString(), *target)
          : comp->Add(shot, static_cast<CompKind>(kind), *target)));
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
  const bool is_search_key =
      watched == &search_ && event->type() == QEvent::KeyPress;
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
