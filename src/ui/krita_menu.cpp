#include "ui/krita_menu.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>

#include <cassert>

#include "edit/doll_library_manager.h"
#include "ui/toast_manager.h"

namespace snapper {

KritaMenu::KritaMenu(DollLibraryManager* library, ToastManager* toasts,
                     QWidget* window)
    : library_(library),
      toasts_(toasts),
      window_(window),
      menu_(tr("Export doll to &Krita")),
      program_action_(tr("Krita &program...")) {
  assert(library_ != nullptr && toasts_ != nullptr);
  assert(window_ != nullptr);
  program_action_.setToolTip(
      tr("Which Krita Snapper opens dolls in, when it isn't found on its "
         "own"));
  connect(&menu_, &QMenu::aboutToShow, this, &KritaMenu::Fill);
  connect(&program_action_, &QAction::triggered, this,
          &KritaMenu::ChooseProgram);
}

void KritaMenu::Open(const QString& doll) {
  assert(!doll.isEmpty());
  const QString path = Prepare(doll);
  if (path.isEmpty()) {
    return;
  }
  const QString file = QFileInfo(path).fileName();
  const auto opened = library_->OpenInKrita(path);
  if (!opened) {
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    toasts_->Problem(tr("Saved %1. %2").arg(file, opened.error().message));
    return;
  }
  toasts_->Say(tr("Opened %1 in Krita.").arg(file));
}

void KritaMenu::Fill() {
  assert(library_ != nullptr);
  menu_.clear();
  const QStringList dolls = library_->Available();
  if (dolls.isEmpty()) {
    QAction* none = menu_.addAction(tr("No dolls in the library"));
    none->setEnabled(false);
    return;
  }
  for (const QString& doll : dolls) {
    menu_.addAction(doll, this, [this, doll] { Open(doll); });
  }
}

void KritaMenu::ChooseProgram() {
  const QString program = QFileDialog::getOpenFileName(
      window_, tr("Krita program"), library_->KritaProgram());
  if (!program.isEmpty()) {
    library_->SetKritaProgram(program);
  }
}

QString KritaMenu::Prepare(const QString& doll) {
  assert(!doll.isEmpty());
  QString path = library_->KritaFileOf(doll);
  const bool is_known = !path.isEmpty() && QFileInfo::exists(path);
  if (!is_known) {
    path = QFileDialog::getSaveFileName(
        window_, tr("Save %1 for Krita").arg(doll),
        library_->SuggestedKritaFile(doll), tr("OpenRaster (*.ora)"));
    if (path.isEmpty()) {
      return {};
    }
    if (!path.endsWith(QStringLiteral(".ora"), Qt::CaseInsensitive)) {
      path += QStringLiteral(".ora");
    }
  } else if (library_->IsKritaFileStale(doll)) {
    const auto answer = QMessageBox::question(
        window_, tr("Open in Krita"),
        tr("%1 changed since %2 was made. Rebuild it? Anything drawn in "
           "it since is replaced.")
            .arg(doll, QFileInfo(path).fileName()),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
        QMessageBox::No);
    if (answer == QMessageBox::Cancel) {
      return {};
    }
    if (answer == QMessageBox::No) {
      return path;
    }
  } else {
    return path;
  }
  const auto written = library_->ExportToKrita(doll, path);
  if (!written) {
    toasts_->Problem(written.error().message);
    return {};
  }
  return path;
}

}  // namespace snapper
