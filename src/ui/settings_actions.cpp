#include "ui/settings_actions.h"

#include <QKeySequence>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "edit/settings_manager.h"
#include "ui/playhead.h"
#include "ui/toast_manager.h"

namespace snapper {

SettingsActions::SettingsActions(const Managers& managers)
    : managers_(managers),
      copy_(tr("Copy settings")),
      paste_(tr("Paste settings")) {
  assert(managers_.IsComplete());
  assert(managers_.settings != nullptr);
  copy_.setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
  paste_.setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V));
  connect(&copy_, &QAction::triggered, this, &SettingsActions::Copy);
  connect(&paste_, &QAction::triggered, this, &SettingsActions::Paste);
  connect(managers_.selection, &SelectionManager::Changed, this,
          &SettingsActions::Refresh);
  connect(managers_.history, &HistoryManager::Changed, this,
          &SettingsActions::Refresh);
  Refresh();
}

void SettingsActions::Copy() {
  assert(managers_.settings != nullptr);
  assert(managers_.selection != nullptr);
  const auto spot = SpotOf(managers_);
  const Frame local = spot ? spot->local : Frame(0);
  const Result<void> done = managers_.settings->Copy(
      managers_.selection->shot(), managers_.selection->picks(), local);
  if (done) {
    managers_.toasts->Cheer(tr("Copied the settings"));
    Refresh();
  } else {
    managers_.toasts->Problem(done.error().message);
  }
}

void SettingsActions::Paste() {
  assert(managers_.settings != nullptr);
  assert(managers_.selection != nullptr);
  const auto spot = SpotOf(managers_);
  const Frame local = spot ? spot->local : Frame(0);
  const Result<void> done = managers_.settings->Paste(
      managers_.selection->shot(), managers_.selection->picks(), local);
  if (!done) {
    managers_.toasts->Problem(done.error().message);
  }
}

void SettingsActions::Refresh() {
  assert(managers_.settings != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const std::set<Pick>& picks = managers_.selection->picks();
  const QString why_copy = managers_.settings->WhyNoCopy(shot, picks);
  const QString why_paste = managers_.settings->WhyNoPaste(shot, picks);
  const bool can_copy = why_copy.isEmpty();
  const bool can_paste = why_paste.isEmpty();
  copy_.setEnabled(can_copy);
  copy_.setToolTip(can_copy ? tr("Copy everything about the picked part "
                                 "except its pose at the playhead")
                            : why_copy);
  paste_.setEnabled(can_paste);
  paste_.setToolTip(can_paste ? tr("Paste the copied settings; onto the "
                                   "same object the groups and drawing "
                                   "come too")
                              : why_paste);
}

}  // namespace snapper
