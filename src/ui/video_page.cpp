#include "ui/video_page.h"

#include <QFileDialog>

#include <cassert>
#include <set>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/reel_manager.h"
#include "edit/selection_manager.h"
#include "ui/form_helpers.h"

namespace snapper {

VideoPage::VideoPage(const Managers& managers)
    : managers_(managers),
      layout_(this),
      split_(Qt::Vertical),
      top_(Qt::Horizontal),
      bottom_layout_(&bottom_),
      bar_layout_(&bar_),
      bin_(managers),
      preview_(managers),
      import_(tr("Import video...")),
      split_button_(tr("Split at playhead")),
      add_track_(tr("Add track")),
      timeline_(managers) {
  assert(managers_.IsComplete());
  layout_.setContentsMargins(0, 0, 0, 0);
  top_.addWidget(&bin_);
  top_.addWidget(&preview_);
  top_.setStretchFactor(0, 1);
  top_.setStretchFactor(1, 4);
  bar_layout_.setContentsMargins(4, 2, 4, 2);
  bar_layout_.addWidget(&import_);
  bar_layout_.addWidget(&split_button_);
  bar_layout_.addWidget(&add_track_);
  bar_layout_.addStretch(1);
  bottom_layout_.setContentsMargins(0, 0, 0, 0);
  bottom_layout_.setSpacing(0);
  bottom_layout_.addWidget(&bar_);
  bottom_layout_.addWidget(&timeline_, 1);
  split_.addWidget(&top_);
  split_.addWidget(&bottom_);
  split_.setStretchFactor(0, 3);
  split_.setStretchFactor(1, 2);
  layout_.addWidget(&split_);
  import_.setToolTip(tr("Puts video files on the bottom track."));
  connect(&import_, &QPushButton::clicked, this, &VideoPage::AskForVideos);
  connect(&split_button_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.reel->SplitAll(
        std::vector<ClipId>(managers_.selection->clips().begin(),
                            managers_.selection->clips().end()),
        managers_.playback->FrameOn(Timeline::kReel))));
  });
  connect(&add_track_, &QPushButton::clicked, this,
          [this] { Report(ProblemOf(managers_.reel->AddTrack())); });
  connect(&timeline_, &ReelTimeline::Problem, this, &VideoPage::Problem);
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, refresh);
  Refresh();
  assert(layout_.count() == 1);
}

void VideoPage::ImportVideos(const QStringList& paths) {
  assert(managers_.reel != nullptr);
  assert(paths.size() < 100000);
  std::set<ClipId> added;
  for (const QString& path : paths) {
    const Frame end = TrackEnd(managers_.history->current(), 0);
    const auto clip = managers_.reel->AddVideo(path, 0, end);
    Report(ProblemOf(clip));
    if (clip) {
      added.insert(*clip);
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
      this, tr("Import video"), QString(),
      tr("Videos (*.mp4 *.mov *.mkv *.webm *.avi *.m4v);;All files (*)"));
  ImportVideos(paths);
}

void VideoPage::Refresh() {
  assert(managers_.reel != nullptr);
  assert(managers_.selection != nullptr);
  const auto& picked = managers_.selection->clips();
  Explain(&split_button_,
          managers_.reel->WhyNoSplit(
              std::vector<ClipId>(picked.begin(), picked.end()),
              managers_.playback->FrameOn(Timeline::kReel)));
  Explain(&add_track_, managers_.reel->WhyNoAddTrack());
}

}  // namespace snapper
