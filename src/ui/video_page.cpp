#include "ui/video_page.h"

#include <QApplication>
#include <QFileDialog>
#include <QKeyEvent>

#include <cassert>
#include <vector>

#include "anim/reel_timeline.h"
#include "edit/document_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "edit/sound_manager.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/theme.h"

namespace snapper {

VideoPage::VideoPage(const Managers& managers)
    : managers_(managers),
      layout_(this),
      split_(Qt::Vertical),
      top_(Qt::Horizontal),
      bin_layout_(&bin_column_),
      bottom_layout_(&bottom_),
      bar_layout_(&bar_),
      bin_(managers),
      new_shot_(tr("+ New shot here")),
      preview_(managers),
      text_panel_(managers),
      subtitle_panel_(managers),
      sound_panel_(managers),
      shot_panel_(managers),
      hint_layout_(&hint_page_),
      import_(tr("Import...")),
      add_text_(tr("T+")),
      tap_key_(QKeySequence(Qt::Key_L), this),
      split_button_(tr("Split at playhead")),
      mark_(tr("Mark cut")),
      add_track_(tr("Add track")),
      transport_(managers),
      timeline_(managers) {
  assert(managers_.IsComplete());
  layout_.setContentsMargins(0, 0, 0, 0);
  bin_layout_.setContentsMargins(0, 0, 0, 0);
  bin_layout_.setSpacing(theme::kRowGap / 2);
  new_shot_.setObjectName("new_shot_here");
  new_shot_.setFocusPolicy(Qt::NoFocus);
  new_shot_.setMinimumHeight(2 * theme::Em(&new_shot_));
  bin_layout_.addWidget(&new_shot_);
  bin_layout_.addWidget(&bin_, 1);
  BuildSide();
  top_.addWidget(&bin_column_);
  top_.addWidget(&preview_);
  top_.addWidget(&side_);
  // The preview takes the golden ratio's long part; the two lists share
  // the short one.
  top_.SetDefaultShares({kGoldenShort / 2, kGoldenLong, kGoldenShort / 2});
  top_.Remember(QStringLiteral("video_top"),
                managers_.document->SettingsPath());
  BuildBar();
  bottom_layout_.setContentsMargins(0, 0, 0, 0);
  bottom_layout_.setSpacing(0);
  bottom_layout_.addWidget(&bar_);
  bottom_layout_.addWidget(&timeline_, 1);
  split_.addWidget(&top_);
  split_.addWidget(&bottom_);
  split_.SetDefaultShares({kGoldenLong, kGoldenShort});
  split_.Remember(QStringLiteral("video_main"),
                  managers_.document->SettingsPath());
  layout_.addWidget(&split_);
  connect(&new_shot_, &QPushButton::clicked, &timeline_,
          &ReelTimeline::NewShotAtPlayhead);
  connect(&bin_, &ShotBin::OpenShotAsked, this,
          [this](ShotId shot) { emit OpenShotAsked(shot, Frame(0)); });
  connect(&timeline_, &ReelTimeline::OpenShotAsked, this,
          &VideoPage::OpenShotAsked);
  connect(&shot_panel_, &ShotClipPanel::OpenShotAsked, this,
          &VideoPage::OpenShotAsked);
  connect(&shot_panel_, &ShotClipPanel::Problem, this, &VideoPage::Problem);
  connect(&sound_panel_, &SoundClipPanel::Problem, this, &VideoPage::Problem);
  connect(&shot_panel_, &ShotClipPanel::EdgeModeChanged, this,
          [this] { Refresh(); });
  connect(&timeline_, &ReelTimeline::EdgeModeChanged, this,
          [this] { Refresh(); });
  connect(&timeline_, &ReelTimeline::Problem, this, &VideoPage::Problem);
  WireText();
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, refresh);
  Refresh();
  assert(layout_.count() == 1);
}

void VideoPage::BuildBar() {
  assert(managers_.reel != nullptr);
  assert(bar_layout_.count() == 0);
  bar_layout_.setContentsMargins(4, 2, 4, 2);
  bar_layout_.addWidget(&transport_);
  const bool can_record = managers_.record != nullptr;
  if (can_record) {
    record_ = std::make_unique<RecordButton>(managers_);
    bar_layout_.addWidget(record_.get());
    connect(record_.get(), &RecordButton::Problem, this, &VideoPage::Problem);
  }
  bar_layout_.addWidget(&import_);
  bar_layout_.addWidget(&add_text_);
  bar_layout_.addWidget(&split_button_);
  bar_layout_.addWidget(&mark_);
  bar_layout_.addWidget(&add_track_);
  import_.setToolTip(tr("Puts video files on the bottom picture track, "
                        "and sound files on a sound track at the "
                        "playhead."));
  connect(&import_, &QPushButton::clicked, this, &VideoPage::AskForVideos);
  connect(&split_button_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.reel->SplitAll(
        std::vector<ClipId>(managers_.selection->clips().begin(),
                            managers_.selection->clips().end()),
        managers_.playback->FrameOn(Timeline::kReel))));
  });
  // M works while the song plays, so cuts can be marked by ear; the
  // button never takes the focus, so it never stops playback keys.
  mark_.setObjectName("mark_cut");
  mark_.setShortcut(Qt::Key_M);
  mark_.setFocusPolicy(Qt::NoFocus);
  mark_.setToolTip(tr("Drops a cut marker at the playhead, or takes away "
                      "the one there (M). Clips snap to markers."));
  connect(&mark_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.reel->ToggleMarker(
        managers_.playback->FrameOn(Timeline::kReel))));
  });
  connect(&add_track_, &QPushButton::clicked, this,
          [this] { Report(ProblemOf(managers_.reel->AddTrack())); });
}

void VideoPage::ImportVideos(const QStringList& paths) {
  assert(managers_.reel != nullptr);
  assert(paths.size() < 100000);
  std::set<ClipId> added;
  QStringList sounds;
  // The bottom picture track sits on top of the sound tracks.
  const int bottom = SoundTrackCount(managers_.history->current().reel);
  for (const QString& path : paths) {
    const bool is_sound = IsSoundFile(path);
    if (is_sound) {
      sounds.append(path);
      continue;
    }
    const Frame end = TrackEnd(managers_.history->current(), bottom);
    const auto clip = managers_.reel->AddVideo(path, bottom, end);
    Report(ProblemOf(clip));
    if (clip) {
      added.insert(*clip);
    }
  }
  const bool has_sounds = !sounds.isEmpty();
  if (has_sounds) {
    const auto made = managers_.reel->AddSounds(
        sounds, -1, managers_.playback->FrameOn(Timeline::kReel));
    Report(ProblemOf(made));
    if (made) {
      added.insert(made->begin(), made->end());
    }
  }
  const bool has_added = !added.empty();
  if (has_added) {
    managers_.selection->PickClips(added, PickMode::kReplace);
  }
}

void VideoPage::AskForVideos() {
  assert(managers_.reel != nullptr);
  assert(managers_.selection != nullptr);
  const QStringList paths = QFileDialog::getOpenFileNames(
      this, tr("Import video or sound"), QString(),
      tr("Videos and sounds (*.mp4 *.mov *.mkv *.webm *.avi *.m4v *.wav "
         "*.mp3 *.ogg *.flac *.m4a *.aac *.opus);;All files (*)"));
  ImportVideos(paths);
}

void VideoPage::WireText() {
  assert(managers_.reel != nullptr);
  assert(managers_.playback != nullptr);
  add_text_.setObjectName("add_text");
  add_text_.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
  add_text_.setFocusPolicy(Qt::NoFocus);
  add_text_.setToolTip(tr("Adds a text clip at the playhead on the highest "
                          "free track (Ctrl+T), ready to type in."));
  connect(&add_text_, &QPushButton::clicked, this, &VideoPage::AddText);
  connect(&preview_, &ReelPreview::Problem, this, &VideoPage::Problem);
  connect(&text_panel_, &ReelTextPanel::Problem, this, &VideoPage::Problem);
  connect(&subtitle_panel_, &SubtitlePanel::Problem, this,
          &VideoPage::Problem);
  connect(&timeline_, &ReelTimeline::TextEditAsked, this,
          &VideoPage::EditText);
  // L taps work while the song plays, like M; letting go is seen by the
  // filter, wherever the focus is.
  tap_key_.setAutoRepeat(false);
  connect(&tap_key_, &QShortcut::activated, this, &VideoPage::PressTap);
  qApp->installEventFilter(this);
}

void VideoPage::AddText() {
  assert(managers_.reel != nullptr);
  assert(managers_.selection != nullptr);
  const auto made =
      managers_.reel->AddText(managers_.playback->FrameOn(Timeline::kReel));
  Report(ProblemOf(made));
  if (!made) {
    return;
  }
  managers_.selection->PickClips({*made}, PickMode::kReplace);
  side_.setCurrentWidget(&text_panel_);
  preview_.EditText(*made);
}

void VideoPage::EditText(ClipId clip) {
  assert(clip.IsValid());
  assert(managers_.playback != nullptr);
  const Clip* found = ClipOf(managers_.history->current().reel, clip);
  const bool is_missing = found == nullptr;
  if (is_missing) {
    return;
  }
  // The words show while they are typed: the playhead goes inside.
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  const bool is_inside = found->start <= now && now < found->end();
  if (!is_inside) {
    managers_.playback->Seek(
        Frame(found->start.index() + found->length.index() / 2));
  }
  preview_.EditText(clip);
}

void VideoPage::PressTap() {
  assert(managers_.reel != nullptr);
  assert(managers_.playback != nullptr);
  const bool is_shown = isVisible();
  if (!is_shown) {
    return;
  }
  const auto made =
      managers_.reel->TapCue(managers_.playback->FrameOn(Timeline::kReel));
  Report(ProblemOf(made));
  if (made) {
    tap_clock_.start();
  }
}

void VideoPage::ReleaseTap() {
  assert(managers_.reel != nullptr);
  assert(tap_clock_.isValid());
  const Frame held(static_cast<int>(tap_clock_.elapsed() * kFramesPerSecond /
                                    1000));
  tap_clock_.invalidate();
  Report(ProblemOf(managers_.reel->ReleaseCue(
      managers_.playback->FrameOn(Timeline::kReel), held)));
}

bool VideoPage::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched != nullptr);
  const bool is_release = event->type() == QEvent::KeyRelease &&
                          tap_clock_.isValid();
  if (is_release) {
    const auto* key = static_cast<const QKeyEvent*>(event);
    const bool is_tap_key = key->key() == Qt::Key_L && !key->isAutoRepeat();
    if (is_tap_key) {
      ReleaseTap();
    }
  }
  return QWidget::eventFilter(watched, event);
}

void VideoPage::Refresh() {
  assert(managers_.reel != nullptr);
  assert(managers_.selection != nullptr);
  const auto& picked = managers_.selection->clips();
  const Frame now = managers_.playback->FrameOn(Timeline::kReel);
  Explain(&add_text_, managers_.reel->WhyNoAddText(now));
  Explain(&split_button_,
          managers_.reel->WhyNoSplit(
              std::vector<ClipId>(picked.begin(), picked.end()), now));
  Explain(&add_track_, managers_.reel->WhyNoAddTrack());
  const Slot span = managers_.reel->NewShotSpan(-1, now);
  const bool has_mark = timeline_.marked().IsValid();
  const Slot target = has_mark ? timeline_.marked() : span;
  Explain(&new_shot_,
          managers_.reel->WhyNoNewShot(has_mark ? timeline_.marked_track() : -1,
                                       target.start, target.end),
          tr("Makes a blank shot as long as the range marked on the reel "
             "(Alt and drag), else the gap at the playhead, and lays it "
             "there. Also in a gap's double-click menu."));
  ShowSide();
}

}  // namespace snapper
