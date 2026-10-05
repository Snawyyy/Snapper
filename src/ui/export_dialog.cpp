#include "ui/export_dialog.h"

#include <cassert>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "ui/form_helpers.h"

namespace snapper {
namespace {

enum Range { kWhole = 0, kLoop = 1, kCustom = 2 };

}  // namespace

ExportDialog::ExportDialog(const Managers& managers, QWidget* parent)
    : QDialog(parent),
      managers_(managers),
      layout_(this),
      browse_(tr("Browse...")),
      export_(tr("Export")),
      cancel_(tr("Close")) {
  assert(managers_.IsComplete());
  setWindowTitle(tr("Export"));
  BuildLayout();
  Wire();
  PickFormat(0);
  assert(layout_.rowCount() > 0);
}

void ExportDialog::BuildLayout() {
  assert(layout_.rowCount() == 0);
  format_.addItem(tr("MP4 video with the song"));
  format_.addItem(tr("GIF, no sound"));
  range_.addItem(tr("Whole master track"));
  range_.addItem(tr("Loop range"));
  range_.addItem(tr("Frames..."));
  for (QSpinBox* frame : {&from_, &to_}) {
    frame->setRange(0, kMaxFrame);
  }
  size_.addItem(tr("Full size"), 1.0);
  size_.addItem(tr("Half size"), 0.5);
  size_.addItem(tr("Quarter size"), 0.25);
  file_row_.addWidget(&path_, 1);
  file_row_.addWidget(&browse_);
  buttons_.addStretch(1);
  buttons_.addWidget(&export_);
  buttons_.addWidget(&cancel_);
  layout_.addRow(tr("Format"), &format_);
  layout_.addRow(tr("Range"), &range_);
  layout_.addRow(tr("From frame"), &from_);
  layout_.addRow(tr("Up to frame"), &to_);
  layout_.addRow(tr("Size"), &size_);
  layout_.addRow(tr("File"), &file_row_);
  layout_.addRow(&progress_);
  layout_.addRow(&status_);
  layout_.addRow(&buttons_);
  status_.setWordWrap(true);
  assert(layout_.rowCount() == 9);
}

void ExportDialog::Wire() {
  ExportManager* exporter = managers_.exporter;
  assert(exporter != nullptr);
  connect(&format_, &QComboBox::currentIndexChanged, this,
          &ExportDialog::PickFormat);
  connect(&range_, &QComboBox::currentIndexChanged, this,
          &ExportDialog::Refresh);
  connect(&from_, &QSpinBox::valueChanged, this, &ExportDialog::Refresh);
  connect(&to_, &QSpinBox::valueChanged, this, &ExportDialog::Refresh);
  connect(&path_, &QLineEdit::textChanged, this, &ExportDialog::Refresh);
  connect(&browse_, &QPushButton::clicked, this, &ExportDialog::Browse);
  connect(&export_, &QPushButton::clicked, this, &ExportDialog::Start);
  connect(&cancel_, &QPushButton::clicked, this, [this, exporter] {
    const bool is_running = exporter->IsRunning();
    if (is_running) {
      exporter->Cancel();
    } else {
      reject();
    }
  });
  connect(exporter, &ExportManager::Progress, this, [this](int done, int all) {
    progress_.setRange(0, all);
    progress_.setValue(done);
  });
  connect(exporter, &ExportManager::Finished, this, [this](const QString& p) {
    status_.setText(tr("Saved %1").arg(p));
    Refresh();
  });
  connect(exporter, &ExportManager::Failed, this, [this](const QString& why) {
    status_.setText(why);
    Refresh();
  });
  connect(exporter, &ExportManager::Cancelled, this, [this] {
    status_.setText(tr("Cancelled; nothing was saved."));
    Refresh();
  });
  connect(managers_.history, &HistoryManager::Changed, this,
          &ExportDialog::Refresh);
}

ExportRequest ExportDialog::Request() const {
  const auto format = static_cast<VideoFormat>(format_.currentIndex());
  ExportRequest request{path_.text().trimmed(), format, Frame(0), Frame(0),
                        size_.currentData().toDouble()};
  const PlaybackManager* playback = managers_.playback;
  const int range = range_.currentIndex();
  const bool is_loop = range == kLoop && playback->HasLoop();
  const bool is_custom = range == kCustom;
  if (is_loop) {
    request.start = playback->loop_start();
    request.end = playback->loop_end();
  } else if (is_custom) {
    request.start = Frame(from_.value());
    request.end = Frame(to_.value() + 1);
  }
  assert(request.scale > 0.0);
  assert(request.start.index() >= 0);
  return request;
}

}  // namespace snapper
