#ifndef SNAPPER_EDIT_EXPORT_MANAGER_H_
#define SNAPPER_EDIT_EXPORT_MANAGER_H_

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>

#include "base/error.h"
#include "base/frame.h"
#include "media/video_encoder.h"

namespace snapper {

class HistoryManager;
class PlaybackManager;

struct ExportRequest final {
  QString path;
  VideoFormat format = VideoFormat::kMp4;
  // Frames [start, end) of what is exported; an end of 0 means the end.
  Frame start;
  Frame end;
  // Output size against the canvas; GIFs are usually smaller.
  double scale = 1.0;
};

// Renders the final video to MP4 (with the song) or GIF on a worker
// thread, from a snapshot of the project, so editing can go on while it
// runs. The final video is the reel once it has a clip, and until then
// the shots one after another. Progress is reported as it goes;
// cancelling leaves no file.
class ExportManager final : public QObject {
  Q_OBJECT

 public:
  ExportManager(HistoryManager* history, PlaybackManager* playback);
  ~ExportManager() override;

  Result<void> Start(const ExportRequest& request);
  void Cancel();
  bool IsRunning() const { return worker_.isRunning(); }
  // Why Export can't start now, for the greyed-out button; empty when
  // it can.
  QString WhyNoExport(VideoFormat format) const;
  // True when the reel is what gets exported.
  bool IsReelExport() const;
  // How many frames the whole export would be.
  Frame ExportLength() const;
  // Why the playback loop can't be the export range: there is none, or
  // it is on the other timeline. Empty when it can.
  QString WhyNoLoop() const;

 signals:
  void Progress(int done, int total);
  void Finished(const QString& path);
  void Failed(const QString& why);
  void Cancelled();

 private:
  void Report();
  void Done();

  HistoryManager* history_;
  PlaybackManager* playback_;
  QFutureWatcher<Result<void>> worker_;
  QTimer progress_timer_;
  std::shared_ptr<std::atomic<bool>> cancel_;
  std::shared_ptr<std::atomic<int>> done_;
  int total_ = 0;
  QString path_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_EXPORT_MANAGER_H_
