#ifndef SNAPPER_UI_VIDEO_PAGE_H_
#define SNAPPER_UI_VIDEO_PAGE_H_

#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"
#include "ui/reel_preview.h"
#include "ui/reel_text_panel.h"
#include "ui/reel_timeline.h"
#include "ui/shot_bin.h"
#include "ui/subtitle_panel.h"
#include "ui/transport_bar.h"

namespace snapper {

// The Video tab, where the final video is cut: the shot list to drag
// from, the preview and the text and subtitle panels on top, the reel's
// timeline below, and the play controls and buttons to bring in video
// files, add text (T+, Ctrl+T), split clips, mark cuts and add tracks.
// L starts the next subtitle line at the playhead; held, it ends that
// line when let go.
class VideoPage final : public QWidget {
  Q_OBJECT

 public:
  explicit VideoPage(const Managers& managers);

  ShotBin* bin() { return &bin_; }
  ReelTimeline* timeline() { return &timeline_; }
  // Puts each file on the bottom track, after what is there; the file
  // dialog's answer goes here.
  void ImportVideos(const QStringList& paths);

 signals:
  void Problem(const QString& why);

 protected:
  // Sees L let go, wherever the focus is, to end a held subtitle line.
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  void AskForVideos();
  void AddText();
  void EditText(ClipId clip);
  void PressTap();
  void ReleaseTap();
  void WireText();
  void Refresh();
  void Report(const QString& problem) { emit Problem(problem); }

  Managers managers_;
  // Containers come before what they hold, so the held widgets leave
  // them before they go.
  QVBoxLayout layout_;
  QSplitter split_;
  QSplitter top_;
  QWidget bottom_;
  QVBoxLayout bottom_layout_;
  QWidget bar_;
  QHBoxLayout bar_layout_;
  QTabWidget side_;
  ShotBin bin_;
  ReelPreview preview_;
  ReelTextPanel text_panel_;
  SubtitlePanel subtitle_panel_;
  QPushButton import_;
  // T+: a text clip at the playhead (Ctrl+T).
  QPushButton add_text_;
  // L: the next subtitle line; how long it has been held.
  QShortcut tap_key_;
  QElapsedTimer tap_clock_;
  QPushButton split_button_;
  QPushButton mark_;
  QPushButton add_track_;
  // Shot clip edges: hold the keys (up) or stretch the timing (down).
  QPushButton edge_mode_;
  TransportBar transport_;
  ReelTimeline timeline_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VIDEO_PAGE_H_
