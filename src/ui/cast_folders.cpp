// CastPanel's library folders: the folder cards, their menus, and moving
// dolls into them.

#include <QIcon>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>

#include <cassert>
#include <memory>

#include "edit/library_folder_manager.h"
#include "ui/cast_panel.h"
#include "ui/deck_paint.h"
#include "ui/library_folder_prompts.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kKeyRole = Qt::UserRole;

// A folder card is a tape case standing on end: a black shell with a
// cream label and a red band, the deck's own look, not the desktop's.
QIcon TapeCaseIcon(const QSize& size) {
  assert(size.width() > 8 && size.height() > 8);
  assert(theme::kPick.isValid());
  QPixmap picture(size);
  picture.fill(Qt::transparent);
  QPainter painter(&picture);
  const int width = size.width() * 3 / 5;
  const QRectF spine((size.width() - width) / 2.0, 2.0, width,
                     size.height() - 4.0);
  deck::PaintCassette(&painter, spine, theme::kPick, QString(), false);
  return QIcon(picture);
}

QString PathIn(const QString& parent, const QString& name) {
  assert(!name.contains('/'));
  assert(!parent.endsWith('/'));
  return parent.isEmpty() ? name : parent + '/' + name;
}

}  // namespace

void CastPanel::AddFolderCards() {
  assert(library_.iconSize().width() > 0);
  LibraryFolderManager* folders = managers_.library_folders;
  const bool has_folders = folders != nullptr;
  if (!has_folders) {
    return;
  }
  assert(library_.count() >= 0);
  const QSignalBlocker quiet(&library_);
  const QIcon folder_icon = TapeCaseIcon(library_.iconSize());
  const QStringList names = folders->FoldersIn(folders->current());
  int at = 0;
  for (const QString& name : names) {
    const QString path = PathIn(folders->current(), name);
    const bool is_hidden = folders->IsHidden(path);
    auto item = std::make_unique<QListWidgetItem>(
        is_hidden ? name + "\n" + tr("hidden") : name);
    item->setData(kFolderRole, path);
    item->setIcon(is_hidden ? QIcon(folder_icon.pixmap(library_.iconSize(),
                                                       QIcon::Disabled))
                            : folder_icon);
    item->setToolTip(is_hidden
                         ? tr("Hidden folder. Double click to open; right "
                              "click to show it again.")
                         : tr("Double click to open; right click for more."));
    // A folder is dropped on, not dragged to the stage.
    item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
    library_.insertItem(at, item.release());
    ++at;
  }
}

QStringList CastPanel::PickedFolders() const {
  assert(library_.count() >= 0);
  assert(managers_.library_folders != nullptr);
  QStringList paths;
  for (const QListWidgetItem* item : library_.selectedItems()) {
    const bool is_folder = IsFolderCard(item);
    if (is_folder) {
      paths.append(item->data(kFolderRole).toString());
    }
  }
  return paths;
}

void CastPanel::OpenFolderCard(const QListWidgetItem* item) {
  assert(item != nullptr);
  assert(managers_.library_folders != nullptr);
  const bool is_folder = IsFolderCard(item);
  if (!is_folder) {
    return;
  }
  const auto opened =
      managers_.library_folders->Open(item->data(kFolderRole).toString());
  Report(ProblemOf(opened));
}

void CastPanel::MoveDollsToFolder(const QStringList& dolls,
                                  const QString& folder) {
  LibraryFolderManager* folders = managers_.library_folders;
  assert(folders != nullptr);
  assert(!dolls.isEmpty());
  const auto moved = folders->MoveDolls(dolls, folder);
  Report(ProblemOf(moved));
  const bool is_lost = moved.has_value() && folders->IsConcealed(folder);
  if (is_lost) {
    Report(tr("Moved into a hidden folder. View > Show hidden folders "
              "shows it."));
  }
}

void CastPanel::DeleteInLibrary() {
  assert(managers_.library != nullptr);
  assert(managers_.library_folders != nullptr);
  const QStringList folders = PickedFolders();
  bool has_dolls = false;
  for (const QListWidgetItem* item : library_.selectedItems()) {
    has_dolls = has_dolls || !IsFolderCard(item);
  }
  if (has_dolls) {
    TakeOut(library_);
    return;
  }
  for (const QString& path : folders) {
    Report(ProblemOf(managers_.library_folders->RemoveFolder(path)));
  }
}

void CastPanel::ShowFolderMenu(const QListWidgetItem* item, QMenu* menu) {
  LibraryFolderManager* folders = managers_.library_folders;
  assert(item != nullptr && menu != nullptr);
  assert(folders != nullptr);
  const QString path = item->data(kFolderRole).toString();
  menu->addAction(tr("Open"), this, [this, item] { OpenFolderCard(item); });
  QAction* hidden = menu->addAction(tr("Hidden"));
  hidden->setCheckable(true);
  hidden->setChecked(folders->IsHidden(path));
  hidden->setToolTip(tr("A hidden folder's dolls leave the library until "
                        "View > Show hidden folders is on."));
  connect(hidden, &QAction::triggered, this, [this, path](bool is_hidden) {
    Report(ProblemOf(managers_.library_folders->SetHidden(path, is_hidden)));
  });
  menu->addAction(tr("Rename..."), this, [this, path] {
    Report(AskRenameFolder(this, managers_.library_folders, path));
  });
  menu->addAction(tr("Delete folder"), this, [this, path] {
    Report(ProblemOf(managers_.library_folders->RemoveFolder(path)));
  })->setToolTip(tr("What is inside moves up one folder."));
}

void CastPanel::AddMoveMenu(const QStringList& dolls, QMenu* menu) {
  LibraryFolderManager* folders = managers_.library_folders;
  assert(menu != nullptr);
  assert(folders != nullptr);
  assert(!dolls.isEmpty());
  QMenu* move = menu->addMenu(tr("Move to folder"));
  const QString here = folders->FolderOf(dolls.front());
  const auto add = [&](const QString& path) {
    const bool is_hidden = !path.isEmpty() && folders->IsHidden(path);
    QAction* action = move->addAction(
        LibraryFolderManager::AddressOf(path) +
        (is_hidden ? tr(" (hidden)") : QString()));
    action->setEnabled(path != here);
    connect(action, &QAction::triggered, this,
            [this, dolls, path] { MoveDollsToFolder(dolls, path); });
  };
  add(QString());
  for (const QString& path : folders->AllFolders()) {
    add(path);
  }
}

void CastPanel::ShowLibraryMenu(const QPoint& at) {
  assert(library_.viewport() != nullptr);
  assert(library_.count() >= 0);
  LibraryFolderManager* folders = managers_.library_folders;
  const QListWidgetItem* item = library_.itemAt(at);
  QMenu menu;
  const bool is_on_card = item != nullptr;
  const bool is_folder = is_on_card && IsFolderCard(item);
  if (is_folder) {
    ShowFolderMenu(item, &menu);
  } else if (is_on_card) {
    const QString name = item->data(kKeyRole).toString();
    assert(!name.isEmpty());
    menu.addAction(tr("Open in Krita"), this,
                   [this, name] { emit OpenInKritaRequested(name); });
    const bool has_folders = folders != nullptr;
    if (has_folders) {
      // The dolls picked, or just this one when it is not among them.
      QStringList dolls = PickedLibraryDolls();
      const bool is_picked = dolls.contains(name);
      if (!is_picked) {
        dolls = {name};
      }
      AddMoveMenu(dolls, &menu);
    }
  }
  const bool can_make = folders != nullptr;
  if (can_make) {
    menu.addSeparator();
    menu.addAction(tr("New folder..."), this, [this] {
      LibraryFolderManager* manager = managers_.library_folders;
      Report(AskNewFolder(this, manager, manager->current()));
    });
  }
  const bool has_entries = !menu.isEmpty();
  if (has_entries) {
    menu.exec(library_.viewport()->mapToGlobal(at));
  }
}

QStringList CastPanel::PickedLibraryDolls() const {
  assert(library_.count() >= 0);
  assert(library_.count() < 100000);
  QStringList dolls;
  for (const QListWidgetItem* item : library_.selectedItems()) {
    const bool is_doll = !IsFolderCard(item);
    if (is_doll) {
      dolls.append(item->data(kKeyRole).toString());
    }
  }
  return dolls;
}

}  // namespace snapper
