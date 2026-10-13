#include "ui/export_dialog.h"

#include <cassert>

#include "anim/master_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/screen_fit.h"
#include "ui/theme.h"

namespace snapper {
namespace {

enum Range { kWhole = 0, kLoop = 1, kCustom = 2 };

// The dialog's size in ems; FitDialog caps it to the screen.
const QSizeF kExportEms(34.0, 30.0);

}  // namespace

ExportDialog::ExportDialog(const Managers& managers, QWidget* parent)
    : QDialog(parent),
      managers_(managers),
      layout_(this),
      browse_(tr("Browse...")),
      sound_only_(tr("Sound only (WAV)...")),
      export_(tr("Export")),
      cancel_(tr("Close")) {
  assert(managers_.IsComplete());
  setWindowTitle(tr("Export"));
  BuildLayout();
  Wire();
  PickFormat(0);
  assert(layout_.count() > 0);
}

void ExportDialog::BuildLayout() {
  assert(layout_.count() == 0);
  // In VideoFormat order; the ones that keep transparency say so.
  format_.addItem(tr("MP4 video with the song"));
  format_.addItem(tr("GIF, no sound (alpha)"));
  format_.addItem(tr("PNG frames (alpha)"));
  format_.addItem(tr("WebM (alpha)"));
  format_.addItem(tr("MOV ProRes (alpha)"));
  assert(format_.count() == kVideoFormatCount);
  format_.setToolTip(
      tr("Alpha formats keep a see-through background. PNG frames go in a "
         "folder, with the song beside them as sound.wav."));
  range_.addItem(tr("Whole master track"));
  range_.addItem(tr("Loop range"));
  range_.addItem(tr("Frames..."));
  for (QSpinBox* frame : {&from_, &to_}) {
    frame->setRange(0, kMaxFrame);
  }
  size_.addItem(tr("Full size"), 1.0);
  size_.addItem(tr("Half size"), 0.5);
  size_.addItem(tr("Quarter size"), 0.25);
  // In SubtitleOut order.
  subtitles_.addItem(tr("Burned in"));
  subtitles_.addItem(tr("Side file"));
  subtitles_.addItem(tr("Both"));
  subtitles_.setToolTip(tr("Burned in draws the cues into the picture. A "
                           "side file goes next to the video: SRT, or "
                           "WebVTT beside a WebM."));
  file_row_.addWidget(&path_, 1);
  file_row_.addWidget(&browse_);
  buttons_.addWidget(&sound_only_);
  buttons_.addStretch(1);
  buttons_.addWidget(&export_);
  buttons_.addWidget(&cancel_);
  // Each section's body is owned by its section, the section by the
  // dialog.
  auto* picture = MakeOwned<QWidget>();
  picture_form_ = SettingsForm(picture);
  picture_form_->addRow(tr("Format"), &format_);
  picture_form_->addRow(&note_);
  note_.setWordWrap(true);
  picture_form_->addRow(tr("Size"), &size_);
  picture_form_->addRow(tr("Subtitles"), &subtitles_);
  auto* frames = MakeOwned<QWidget>();
  frames_form_ = SettingsForm(frames);
  frames_form_->addRow(tr("Range"), &range_);
  AddNumberRow(frames_form_, tr("From frame"), &from_, nullptr);
  AddNumberRow(frames_form_, tr("Up to frame"), &to_, nullptr);
  auto* file = MakeOwned<QWidget>();
  SettingsForm(file)->addRow(tr("Save to"), &file_row_);
  layout_.setSpacing(theme::kRowGap);
  layout_.addWidget(PlatedSection(tr("Picture"), picture, this));
  layout_.addWidget(PlatedSection(tr("Frames"), frames, this));
  layout_.addWidget(PlatedSection(tr("File"), file, this));
  layout_.addStretch(1);
  layout_.addWidget(&progress_);
  layout_.addWidget(&status_);
  layout_.addLayout(&buttons_);
  status_.setWordWrap(true);
  assert(layout_.count() == 7);
}

void ExportDialog::Open() {
  assert(managers_.exporter != nullptr);
  assert(layout_.count() > 0);
  const bool is_closed = !isVisible();
  if (is_closed) {
    FitDialog(this, kExportEms);
  }
  show();
  raise();
  activateWindow();
}

void ExportDialog::Wire() {
  ExportManager* exporter = managers_.exporter;
  assert(exporter != nullptr);
  assert(managers_.history != nullptr);
  connect(&format_, &ChoiceRow::currentIndexChanged, this,
          &ExportDialog::PickFormat);
  connect(&range_, &ChoiceRow::currentIndexChanged, this,
          &ExportDialog::Refresh);
  connect(&from_, &QSpinBox::valueChanged, this, &ExportDialog::Refresh);
  connect(&to_, &QSpinBox::valueChanged, this, &ExportDialog::Refresh);
  connect(&path_, &QLineEdit::textChanged, this, &ExportDialog::Refresh);
  connect(&browse_, &QPushButton::clicked, this, &ExportDialog::Browse);
  connect(&export_, &QPushButton::clicked, this, &ExportDialog::Start);
  connect(&sound_only_, &QPushButton::clicked, this,
          &ExportDialog::ExportSound);
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
                        size_.currentData().toDouble(),
                        static_cast<SubtitleOut>(subtitles_.currentIndex())};
  const PlaybackManager* playback = managers_.playback;
  const int range = range_.currentIndex();
  const bool is_loop =
      range == kLoop && managers_.exporter->WhyNoLoop().isEmpty();
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
