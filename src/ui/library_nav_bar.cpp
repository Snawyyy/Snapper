#include "ui/library_nav_bar.h"

#include <cassert>

#include "edit/library_folder_manager.h"
#include "ui/form_helpers.h"
#include "ui/library_folder_prompts.h"

namespace snapper {

LibraryNavBar::LibraryNavBar(const Managers& managers)
    : managers_(managers), layout_(this) {
  assert(managers_.library_folders != nullptr);
  assert(managers_.IsComplete());
  LibraryFolderManager* folders = managers_.library_folders;
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(2);
  back_.setArrowType(Qt::LeftArrow);
  forward_.setArrowType(Qt::RightArrow);
  up_.setArrowType(Qt::UpArrow);
  new_folder_.setText(tr("+"));
  for (QToolButton* button : {&back_, &forward_, &up_, &new_folder_}) {
    layout_.addWidget(button);
  }
  layout_.insertWidget(3, &address_, 1);
  address_.setClearButtonEnabled(false);
  address_.setToolTip(tr("Where you are. Type a folder and press Enter to "
                         "go there."));
  connect(&back_, &QToolButton::clicked, folders,
          &LibraryFolderManager::Back);
  connect(&forward_, &QToolButton::clicked, folders,
          &LibraryFolderManager::Forward);
  connect(&up_, &QToolButton::clicked, folders, &LibraryFolderManager::Up);
  connect(&new_folder_, &QToolButton::clicked, this, [this] {
    LibraryFolderManager* manager = managers_.library_folders;
    Report(AskNewFolder(this, manager, manager->current()));
  });
  connect(&address_, &QLineEdit::returnPressed, this,
          &LibraryNavBar::OpenTyped);
  // Left without Enter, the address goes back to where you are.
  connect(&address_, &QLineEdit::editingFinished, this,
          &LibraryNavBar::Refresh);
  connect(folders, &LibraryFolderManager::Changed, this,
          &LibraryNavBar::Refresh);
  Refresh();
}

void LibraryNavBar::Refresh() {
  assert(managers_.library_folders != nullptr);
  assert(layout_.count() == 5);
  const LibraryFolderManager& folders = *managers_.library_folders;
  address_.setText(LibraryFolderManager::AddressOf(folders.current()));
  Explain(&back_, folders.WhyNoBack(), tr("Back to the folder before."));
  Explain(&forward_, folders.WhyNoForward(), tr("Forward again."));
  Explain(&up_, folders.WhyNoUp(), tr("Up to the folder this one is in."));
  Explain(&new_folder_, QString(),
          tr("Make a folder in the one that is open."));
}

void LibraryNavBar::OpenTyped() {
  assert(managers_.library_folders != nullptr);
  assert(address_.text().size() < 100000);
  const auto opened = managers_.library_folders->OpenAddress(address_.text());
  if (!opened) {
    Report(opened.error().message);
  }
  Refresh();
}

}  // namespace snapper
