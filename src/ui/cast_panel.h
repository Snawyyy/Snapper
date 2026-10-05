#ifndef SNAPPER_UI_CAST_PANEL_H_
#define SNAPPER_UI_CAST_PANEL_H_

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"
#include "ui/problem.h"

namespace snapper {

// Who is in the picked shot. The top half is the doll library: import a
// doll, reload it after a Krita re-export (marked "new art"), or put it
// on the stage. The bottom half lists the shot's layers, top first:
// pick one, tick to show or hide it, add pictures, text and effects,
// copy, delete and restack.
class CastPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit CastPanel(const Managers& managers);

 signals:
  void Problem(const QString& why);

 private:
  void BuildLayout();
  void Wire();
  void RefreshLibrary();
  void RefreshLayers();
  void RefreshButtons();
  void Import();
  void Reload();
  void Place();
  void AddPicture();
  void AddText();
  void Restack(int step);
  void PickLayer();
  void ToggleShown(QListWidgetItem* item);
  QString PickedDoll() const;
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  // Library dolls re-exported since they were last taken.
  QSet<QString> stale_;
  QVBoxLayout layout_;
  QLabel library_title_;
  QListWidget library_;
  QHBoxLayout library_buttons_;
  QPushButton import_;
  QPushButton reload_;
  QPushButton place_;
  QLabel layers_title_;
  QListWidget layers_;
  QHBoxLayout layer_buttons_;
  QToolButton add_;
  QMenu add_menu_;
  QPushButton copy_;
  QPushButton remove_;
  QPushButton up_;
  QPushButton down_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CAST_PANEL_H_
