// ExportDialog's checks and buttons.

#include <QFileDialog>
#include <QFileInfo>
#include <QSignalBlocker>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "ui/export_dialog.h"
#include "ui/form_helpers.h"

namespace snapper {
namespace {

enum Range { kWhole = 0, kLoop = 1, kCustom = 2 };

QString Suffix(VideoFormat format) {
  assert(format == VideoFormat::kMp4 || format == VideoFormat::kGif);
  const bool is_gif = format == VideoFormat::kGif;
  return is_gif ? QStringLiteral("gif") : QStringLiteral("mp4");
}

}  // namespace

QString ExportDialog::WhyNotReady() const {
  assert(managers_.exporter != nullptr);
  const ExportRequest request = Request();
  const QString busy = managers_.exporter->WhyNoExport(request.format);
  const bool has_path = !request.path.isEmpty();
  const QString no_loop = range_.currentIndex() == kLoop
                              ? managers_.exporter->WhyNoLoop()
                              : QString();
  const bool is_loop_missing = !no_loop.isEmpty();
  const bool is_range_empty =
      range_.currentIndex() == kCustom && !(request.start < request.end);
  const bool is_blocked = !busy.isEmpty();
  if (is_blocked) {
    return busy;
  }
  if (is_loop_missing) {
    return no_loop;
  }
  if (is_range_empty) {
    return tr("The last frame must come after the first.");
  }
  return has_path ? QString() : tr("Choose a file to save to.");
}

void ExportDialog::Browse() {
  const VideoFormat format = Request().format;
  assert(format == VideoFormat::kMp4 || format == VideoFormat::kGif);
  const QString suffix = Suffix(format);
  const QString start = path_.text().isEmpty()
                            ? managers_.history->current().name + "." + suffix
                            : path_.text();
  const QString path = QFileDialog::getSaveFileName(
      this, tr("Export to"), start,
      tr("%1 files (*.%2)").arg(suffix.toUpper(), suffix));
  const bool is_chosen = !path.isEmpty();
  if (is_chosen) {
    const bool has_suffix = QFileInfo(path).suffix().toLower() == suffix;
    path_.setText(has_suffix ? path : path + "." + suffix);
  }
}

void ExportDialog::Start() {
  assert(managers_.exporter != nullptr);
  const QString why_not = WhyNotReady();
  const bool can_start = why_not.isEmpty();
  if (!can_start) {
    status_.setText(why_not);
    return;
  }
  const auto started = managers_.exporter->Start(Request());
  status_.setText(started ? tr("Exporting...") : started.error().message);
  Refresh();
}

void ExportDialog::Refresh() {
  assert(managers_.exporter != nullptr);
  const bool is_running = managers_.exporter->IsRunning();
  const bool is_custom = range_.currentIndex() == kCustom;
  layout_.setRowVisible(&from_, is_custom);
  layout_.setRowVisible(&to_, is_custom);
  const int last =
      std::max(0, managers_.exporter->ExportLength().index() - 1);
  range_.setItemText(kWhole, managers_.exporter->IsReelExport()
                                 ? tr("Whole final video")
                                 : tr("Whole master track"));
  {
    const QSignalBlocker quiet_from(from_);
    const QSignalBlocker quiet_to(to_);
    from_.setMaximum(last);
    to_.setMaximum(last);
  }
  Explain(&export_, is_running ? tr("Already exporting.") : WhyNotReady());
  cancel_.setText(is_running ? tr("Cancel export") : tr("Close"));
  for (QWidget* field : {static_cast<QWidget*>(&format_),
                         static_cast<QWidget*>(&range_),
                         static_cast<QWidget*>(&size_),
                         static_cast<QWidget*>(&path_),
                         static_cast<QWidget*>(&browse_)}) {
    field->setEnabled(!is_running);
  }
  assert(is_running || cancel_.text() == tr("Close"));
}

void ExportDialog::PickFormat(int index) {
  assert(index >= 0 && index <= 1);
  const auto format = static_cast<VideoFormat>(index);
  // GIFs are usually shared small; MP4 goes out full size.
  size_.setCurrentIndex(format == VideoFormat::kGif ? 1 : 0);
  const QString path = path_.text();
  const bool has_path = !path.isEmpty();
  if (has_path) {
    const QFileInfo info(path);
    path_.setText(info.dir().filePath(info.completeBaseName() + "." +
                                      Suffix(format)));
  }
  Refresh();
}

}  // namespace snapper
