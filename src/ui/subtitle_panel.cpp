#include "ui/subtitle_panel.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QSignalBlocker>

#include <cassert>
#include <memory>
#include <set>
#include <variant>

#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {
namespace {

// Line breaks inside a cue show in the list as this, and turn back.
const QString kBreak = QStringLiteral(" / ");

enum Column { kTime = 0, kWords = 1 };

}  // namespace

SubtitlePanel::SubtitlePanel(const Managers& managers)
    : managers_(managers),
      layout_(this),
      use_lines_(tr("Use these lines")),
      tap_(tr("Next line (L)")),
      cues_(0, 2),
      replace_(tr("Replace all")),
      import_(tr("Import SRT or WebVTT...")) {
  assert(managers_.IsComplete());
  Build();
  Wire();
  Refresh();
  assert(layout_.count() > 0);
}

void SubtitlePanel::Build() {
  assert(layout_.count() == 0);
  help_.setWordWrap(true);
  help_.setText(tr("Paste the lines, play the song and press L as each "
                   "line starts. Hold L to keep a line on until you let "
                   "go."));
  lines_.setPlaceholderText(tr("One line per cue"));
  lines_.setMaximumHeight(90);
  tap_.setFocusPolicy(Qt::NoFocus);
  line_row_.addWidget(&use_lines_);
  line_row_.addWidget(&tap_);
  waiting_.setWordWrap(true);
  cues_.setHorizontalHeaderLabels({tr("Time"), tr("Words")});
  cues_.horizontalHeader()->setStretchLastSection(true);
  cues_.verticalHeader()->hide();
  cues_.setSelectionBehavior(QAbstractItemView::SelectRows);
  find_.setPlaceholderText(tr("Find"));
  with_.setPlaceholderText(tr("Replace with"));
  find_row_.addWidget(&find_);
  find_row_.addWidget(&with_);
  find_row_.addWidget(&replace_);
  layout_.addWidget(&help_);
  layout_.addWidget(&lines_);
  layout_.addLayout(&line_row_);
  layout_.addWidget(&waiting_);
  layout_.addWidget(&cues_, 1);
  layout_.addLayout(&find_row_);
  layout_.addWidget(&import_);
  assert(cues_.columnCount() == 2);
}

void SubtitlePanel::Wire() {
  assert(managers_.history != nullptr);
  assert(managers_.reel != nullptr);
  connect(managers_.history, &HistoryManager::Changed, this,
          &SubtitlePanel::Refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, [this] {
    Explain(&tap_, managers_.reel->WhyNoTap(
                       managers_.playback->FrameOn(Timeline::kReel)));
  });
  connect(&use_lines_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.reel->SetSubtitleLines(lines_.toPlainText())));
  });
  connect(&tap_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.reel->TapCue(
        managers_.playback->FrameOn(Timeline::kReel))));
  });
  connect(&cues_, &QTableWidget::itemChanged, this, &SubtitlePanel::TakeEdit);
  connect(&cues_, &QTableWidget::cellClicked, this,
          [this](int row, int) { Jump(row); });
  connect(&replace_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(
        managers_.reel->ReplaceInCues(find_.text(), with_.text())));
  });
  connect(&import_, &QPushButton::clicked, this, &SubtitlePanel::Import);
}

void SubtitlePanel::Refresh() {
  assert(managers_.history != nullptr);
  assert(managers_.reel != nullptr);
  const Reel& reel = managers_.history->current().reel;
  const size_t waiting = reel.subtitle_lines.size();
  waiting_.setText(waiting == 0
                       ? tr("No lines waiting.")
                       : tr("%1 lines waiting. Next: %2")
                             .arg(waiting)
                             .arg(reel.subtitle_lines.front()));
  Explain(&tap_, managers_.reel->WhyNoTap(
                     managers_.playback->FrameOn(Timeline::kReel)));
  Explain(&replace_, SubtitleTrack(reel) < 0
                         ? tr("There are no cues yet.")
                         : QString());
  FillList();
}

void SubtitlePanel::FillList() {
  assert(managers_.history != nullptr);
  assert(cues_.columnCount() == 2);
  const Reel& reel = managers_.history->current().reel;
  const int track = SubtitleTrack(reel);
  const QSignalBlocker quiet(&cues_);
  cues_.setRowCount(0);
  if (track < 0) {
    return;
  }
  for (const Clip& clip : reel.tracks[static_cast<size_t>(track)].clips) {
    const int row = cues_.rowCount();
    cues_.insertRow(row);
    auto time = std::make_unique<QTableWidgetItem>(TimeText(clip.start));
    time->setFlags(time->flags() & ~Qt::ItemIsEditable);
    time->setData(Qt::UserRole, clip.id.value());
    auto words = std::make_unique<QTableWidgetItem>(
        QString(std::get<TextSource>(clip.source).text)
            .replace(QLatin1Char('\n'), kBreak));
    words->setData(Qt::UserRole, clip.id.value());
    // The table owns its items.
    cues_.setItem(row, kTime, time.release());
    cues_.setItem(row, kWords, words.release());
  }
}

void SubtitlePanel::TakeEdit(QTableWidgetItem* item) {
  assert(item != nullptr);
  assert(managers_.reel != nullptr);
  const bool is_words = item->column() == kWords;
  if (!is_words) {
    return;
  }
  const ClipId clip(item->data(Qt::UserRole).toInt());
  QString text = item->text();
  text.replace(kBreak, QStringLiteral("\n"));
  Report(ProblemOf(managers_.reel->SetText(clip, text)));
}

void SubtitlePanel::Jump(int row) {
  assert(row >= 0);
  assert(managers_.selection != nullptr);
  const QTableWidgetItem* item = cues_.item(row, kTime);
  if (item == nullptr) {
    return;
  }
  const ClipId clip(item->data(Qt::UserRole).toInt());
  const Clip* found = ClipOf(managers_.history->current().reel, clip);
  if (found == nullptr) {
    return;
  }
  managers_.playback->Seek(found->start);
  managers_.selection->PickClips({clip}, PickMode::kReplace);
}

void SubtitlePanel::Import() {
  assert(managers_.reel != nullptr);
  assert(managers_.selection != nullptr);
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Import subtitles"), QString(),
      tr("Subtitles (*.srt *.vtt);;All files (*)"));
  if (path.isEmpty()) {
    return;
  }
  const auto made = managers_.reel->ImportSubtitles(path);
  Report(ProblemOf(made));
  if (made) {
    managers_.selection->PickClips(std::set<ClipId>(made->begin(), made->end()),
                                   PickMode::kReplace);
  }
}

}  // namespace snapper
