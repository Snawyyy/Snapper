#include "ui/file_menu.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QMessageBox>

#include <cassert>

#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "io/project_file.h"
#include "ui/new_project_dialog.h"

namespace snapper {
namespace {

QString ProjectFilter() {
  return QObject::tr("Snapper projects (*.%1)")
      .arg(QLatin1String(kProjectSuffix));
}

}  // namespace

FileMenu::FileMenu(DocumentManager* document, HistoryManager* history,
                   QWidget* window)
    : document_(document),
      history_(history),
      window_(window),
      menu_(tr("&File")),
      recent_menu_(tr("Open &recent")),
      new_action_(tr("&New...")),
      open_action_(tr("&Open...")),
      save_action_(tr("&Save")),
      save_as_action_(tr("Save &as...")),
      song_action_(tr("Choose so&ng...")),
      remove_song_action_(tr("Remove song")),
      quit_action_(tr("&Quit")) {
  assert(document_ != nullptr && history_ != nullptr);
  assert(window_ != nullptr);
  new_action_.setShortcut(QKeySequence::New);
  open_action_.setShortcut(QKeySequence::Open);
  save_action_.setShortcut(QKeySequence::Save);
  save_as_action_.setShortcut(QKeySequence::SaveAs);
  quit_action_.setShortcut(QKeySequence::Quit);
  menu_.setToolTipsVisible(true);
  menu_.addActions({&new_action_, &open_action_});
  menu_.addMenu(&recent_menu_);
  menu_.addSeparator();
  menu_.addActions({&save_action_, &save_as_action_});
  menu_.addSeparator();
  menu_.addActions({&song_action_, &remove_song_action_});
  menu_.addSeparator();
  menu_.addAction(&quit_action_);
  connect(&new_action_, &QAction::triggered, this, &FileMenu::New);
  connect(&open_action_, &QAction::triggered, this, &FileMenu::Open);
  connect(&save_action_, &QAction::triggered, this, [this] { Save(); });
  connect(&save_as_action_, &QAction::triggered, this,
          [this] { SaveAs(); });
  connect(&song_action_, &QAction::triggered, this, &FileMenu::ChooseSong);
  connect(&remove_song_action_, &QAction::triggered, this,
          &FileMenu::RemoveSong);
  connect(&quit_action_, &QAction::triggered, window_, &QWidget::close);
  connect(document_, &DocumentManager::RecentFilesChanged, this,
          &FileMenu::RefreshRecent);
  connect(history_, &HistoryManager::Changed, this, &FileMenu::Refresh);
  RefreshRecent();
  Refresh();
}

bool FileMenu::ConfirmDiscard() {
  assert(history_ != nullptr);
  assert(window_ != nullptr);
  const bool is_saved = !history_->IsDirty();
  if (is_saved) {
    return true;
  }
  const auto answer = QMessageBox::question(
      window_, tr("Unsaved changes"),
      tr("Save the changes to %1 first?").arg(history_->current().name),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  const bool wants_save = answer == QMessageBox::Save;
  if (wants_save) {
    return Save();
  }
  return answer == QMessageBox::Discard;
}

void FileMenu::OfferRecovery() {
  assert(document_ != nullptr);
  assert(window_ != nullptr);
  const bool has_autosave = document_->HasAutosave();
  if (!has_autosave) {
    return;
  }
  const auto answer = QMessageBox::question(
      window_, tr("Recover work"),
      tr("Snapper closed without saving last time. Recover that work?"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
  const bool wants_it = answer == QMessageBox::Yes;
  if (wants_it) {
    Report(document_->Recover());
  } else {
    document_->DiscardAutosave();
  }
}

void FileMenu::New() {
  assert(document_ != nullptr);
  assert(window_ != nullptr);
  NewProjectDialog dialog(window_);
  const bool is_chosen = ConfirmDiscard() && dialog.exec() == QDialog::Accepted;
  if (is_chosen) {
    Report(document_->New(dialog.name(), dialog.canvas()));
  }
}

void FileMenu::Open() {
  assert(document_ != nullptr);
  assert(window_ != nullptr);
  const bool may_replace = ConfirmDiscard();
  if (!may_replace) {
    return;
  }
  const QString path = QFileDialog::getOpenFileName(
      window_, tr("Open project"), QFileInfo(document_->path()).path(),
      ProjectFilter());
  const bool is_chosen = !path.isEmpty();
  if (is_chosen) {
    Report(document_->Open(path));
  }
}

void FileMenu::OpenPath(const QString& path) {
  assert(document_ != nullptr);
  assert(!path.isEmpty());
  const bool may_replace = ConfirmDiscard();
  if (may_replace) {
    Report(document_->Open(path));
  }
}

bool FileMenu::Save() {
  assert(document_ != nullptr);
  assert(history_ != nullptr);
  const bool has_place = document_->HasPath();
  if (!has_place) {
    return SaveAs();
  }
  const Result<void> saved = document_->Save();
  Report(saved);
  return saved.has_value();
}

bool FileMenu::SaveAs() {
  assert(document_ != nullptr);
  assert(window_ != nullptr);
  const QString suggested = document_->HasPath()
                                ? document_->path()
                                : history_->current().name;
  const QString path = QFileDialog::getSaveFileName(
      window_, tr("Save project"), suggested, ProjectFilter());
  const bool is_chosen = !path.isEmpty();
  if (!is_chosen) {
    return false;
  }
  const Result<void> saved = document_->SaveAs(path);
  Report(saved);
  return saved.has_value();
}

void FileMenu::ChooseSong() {
  assert(document_ != nullptr);
  assert(window_ != nullptr);
  const QString path = QFileDialog::getOpenFileName(
      window_, tr("Choose song"), QString(),
      tr("Sound (*.wav *.mp3 *.flac *.ogg *.opus *.m4a *.aac);;"
         "All files (*)"));
  const bool is_chosen = !path.isEmpty();
  if (is_chosen) {
    Report(document_->SetSong(path));
  }
}

void FileMenu::RemoveSong() {
  assert(document_ != nullptr);
  assert(history_ != nullptr);
  Report(document_->SetSong(QString()));
}

void FileMenu::RefreshRecent() {
  assert(document_ != nullptr);
  recent_menu_.clear();
  const QStringList recent = document_->RecentFiles();
  for (const QString& path : recent) {
    QAction* item = recent_menu_.addAction(QFileInfo(path).fileName());
    item->setToolTip(path);
    connect(item, &QAction::triggered, this,
            [this, path] { OpenPath(path); });
  }
  recent_menu_.setEnabled(!recent.isEmpty());
  assert(recent_menu_.actions().size() == recent.size());
}

void FileMenu::Refresh() {
  assert(history_ != nullptr);
  const bool has_song = !history_->current().song.isEmpty();
  remove_song_action_.setEnabled(has_song);
  remove_song_action_.setToolTip(has_song ? QString()
                                          : tr("There is no song yet."));
  assert(remove_song_action_.isEnabled() == has_song);
}

void FileMenu::Report(const Result<void>& result) const {
  assert(window_ != nullptr);
  const bool is_failed = !result.has_value();
  if (is_failed) {
    QMessageBox::warning(window_, tr("Snapper"), result.error().message);
  }
  assert(result.has_value() != is_failed);
}

}  // namespace snapper
