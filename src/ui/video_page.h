#ifndef SNAPPER_UI_VIDEO_PAGE_H_
#define SNAPPER_UI_VIDEO_PAGE_H_

#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"
#include "ui/reel_preview.h"
#include "ui/reel_timeline.h"
#include "ui/shot_bin.h"

namespace snapper {

// The Video tab, where the final video is cut: the shot list to drag
// from and the preview on top, the reel's timeline below, and buttons
// to bring in video files, split clips and add tracks.
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

 private:
  void AskForVideos();
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
  ShotBin bin_;
  ReelPreview preview_;
  QPushButton import_;
  QPushButton split_button_;
  QPushButton add_track_;
  ReelTimeline timeline_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_VIDEO_PAGE_H_
