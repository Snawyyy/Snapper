#ifndef SNAPPER_UI_SETTINGS_ACTIONS_H_
#define SNAPPER_UI_SETTINGS_ACTIONS_H_

#include <QAction>
#include <QList>
#include <QObject>

#include "ui/managers.h"

namespace snapper {

// The Edit menu's Copy settings (Ctrl+Shift+C) and Paste settings
// (Ctrl+Shift+V): they pass the picked part and the playhead to the
// settings manager and say so with a toast, or say why not. Each greys
// out, with the reason in its tooltip, when it can't act.
class SettingsActions final : public QObject {
  Q_OBJECT

 public:
  explicit SettingsActions(const Managers& managers);

  // Both actions, to put in a menu.
  QList<QAction*> actions() { return {&copy_, &paste_}; }

 private:
  void Copy();
  void Paste();
  void Refresh();

  Managers managers_;
  QAction copy_;
  QAction paste_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SETTINGS_ACTIONS_H_
