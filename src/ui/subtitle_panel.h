#ifndef SNAPPER_UI_SUBTITLE_PANEL_H_
#define SNAPPER_UI_SUBTITLE_PANEL_H_

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

// The subtitle track beside the reel: paste the lines (lyrics, a
// script) and tap L while the song plays to time them; every cue in a
// list (time and words, the words editable, a click jumps there); find
// and replace across cues; import an SRT or WebVTT file. Edits go
// through ReelManager.
class SubtitlePanel final : public QWidget {
  Q_OBJECT

 public:
  explicit SubtitlePanel(const Managers& managers);

 signals:
  void Problem(const QString& why);

 private:
  void Build();
  void Wire();
  void Refresh();
  void FillList();
  void TakeEdit(QTableWidgetItem* item);
  void Jump(int row);
  void Import();
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  QVBoxLayout layout_;
  QLabel help_;
  QPlainTextEdit lines_;
  QHBoxLayout line_row_;
  QPushButton use_lines_;
  QPushButton tap_;
  QLabel waiting_;
  QTableWidget cues_;
  QHBoxLayout find_row_;
  QLineEdit find_;
  QLineEdit with_;
  QPushButton replace_;
  QPushButton import_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SUBTITLE_PANEL_H_
