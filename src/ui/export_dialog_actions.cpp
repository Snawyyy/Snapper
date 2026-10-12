// ExportDialog's checks and buttons.

#include <QDir>
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

// path with the ending format wants: none for a PNG folder.
QString WithSuffix(const QString& path, VideoFormat format) {
  const QFileInfo info(path);
  const QString suffix = FormatSuffix(format);
  const QString base = info.dir().filePath(info.completeBaseName());
  return suffix.isEmpty() ? base : base + "." + suffix;
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
  assert(range_.currentIndex() >= kWhole && range_.currentIndex() <= kCustom);
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
  const QString suffix = FormatSuffix(format);
  assert(managers_.history != nullptr);
  const bool is_folder = suffix.isEmpty();
  if (is_folder) {
    // PNG frames: a new folder named after the project, inside the one
    // picked.
    const QString where = QFileDialog::getExistingDirectory(
        this, tr("Put the frames folder in"),
        QFileInfo(path_.text()).dir().path());
    const bool is_picked = !where.isEmpty();
    if (is_picked) {
      path_.setText(QDir(where).filePath(managers_.history->current().name +
                                         tr(" frames")));
    }
    return;
  }
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
  assert(managers_.history != nullptr);
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
  const QString note = managers_.exporter->FormatNote(Request().format);
  note_.setText(note);
  layout_.setRowVisible(&note_, !note.isEmpty());
  cancel_.setText(is_running ? tr("Cancel export") : tr("Close"));
  for (QWidget* field : {static_cast<QWidget*>(&format_),
                         static_cast<QWidget*>(&range_),
                         static_cast<QWidget*>(&size_),
                         static_cast<QWidget*>(&path_),
                         static_cast<QWidget*>(&browse_)}) {
    Explain(field, is_running ? tr("Not while exporting. Cancel first.")
                              : QString());
  }
  assert(is_running || cancel_.text() == tr("Close"));
}

void ExportDialog::PickFormat(int index) {
  assert(index >= 0 && index < kVideoFormatCount);
  const auto format = static_cast<VideoFormat>(index);
  assert(size_.count() >= 2);
  // GIFs are usually shared small; the rest go out full size.
  size_.setCurrentIndex(format == VideoFormat::kGif ? 1 : 0);
  const QString path = path_.text();
  const bool has_path = !path.isEmpty();
  if (has_path) {
    path_.setText(WithSuffix(path, format));
  }
  Refresh();
}

}  // namespace snapper
