#include "ui/video_page.h"

#include <QTabBar>

#include <cassert>
#include <variant>
#include <vector>

#include "anim/reel_timeline.h"
#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/panel_kit.h"

namespace snapper {
namespace {

// The side tabs that follow the pick (Text, Subtitles, Sound clip, Shot
// clip, Nothing picked); any tab after them, the Voice panel, is always
// shown.
constexpr int kPickTabs = 5;

}  // namespace

SidePanels SidePanelsFor(const Reel& reel, const std::set<ClipId>& picked) {
  assert(picked.size() < 1000000);
  assert(reel.tracks.size() < 100000);
  SidePanels panels;
  const bool is_none = picked.empty();
  if (is_none) {
    panels.subtitles = true;
    return panels;
  }
  int texts = 0;
  int cues = 0;
  int sounds = 0;
  int shots = 0;
  const int subtitle_track = SubtitleTrack(reel);
  for (const ClipId id : picked) {
    const Clip* clip = ClipOf(reel, id);
    const bool is_gone = clip == nullptr;
    if (is_gone) {
      continue;
    }
    const bool is_text = IsTextClip(*clip);
    texts += is_text ? 1 : 0;
    cues += is_text && FindClip(reel, id).track == subtitle_track ? 1 : 0;
    sounds += IsSoundClip(*clip) ? 1 : 0;
    shots += std::holds_alternative<ShotSource>(clip->source) ? 1 : 0;
  }
  const int count = static_cast<int>(picked.size());
  panels.text = texts == count;
  panels.subtitles = panels.text && cues > 0;
  panels.sound = sounds == count;
  panels.shot = shots == count;
  return panels;
}

void VideoPage::BuildSide() {
  assert(side_.count() == 0);
  assert(hint_layout_.count() == 0);
  hint_layout_.addWidget(
      EmptyHint(tr("Pick a text, sound or shot clip on the reel to change "
                   "it here."),
                &hint_page_));
  hint_layout_.addStretch(1);
  side_.addTab(&text_panel_, tr("Text"));
  side_.addTab(&subtitle_panel_, tr("Subtitles"));
  side_.addTab(&sound_panel_, tr("Sound clip"));
  side_.addTab(&shot_panel_, tr("Shot clip"));
  side_.addTab(&hint_page_, tr("Nothing picked"));
  const bool can_speak = managers_.voices != nullptr &&
                         managers_.voice_gen != nullptr;
  if (can_speak) {
    voice_ = std::make_unique<VoicePanel>(managers_);
    side_.addTab(voice_.get(), tr("Voice"));
    connect(voice_.get(), &VoicePanel::Problem, this, &VideoPage::Problem);
  }
}

void VideoPage::ShowSide() {
  assert(managers_.selection != nullptr);
  assert(side_.count() >= kPickTabs);
  const SidePanels panels = SidePanelsFor(managers_.history->current().reel,
                                          managers_.selection->clips());
  const bool follows[kPickTabs] = {panels.text, panels.subtitles,
                                   panels.sound, panels.shot,
                                   panels.IsEmpty()};
  int shown = 0;
  std::vector<bool> shows;
  for (int tab = 0; tab < side_.count(); ++tab) {
    const bool is_shown = tab >= kPickTabs || follows[tab];
    shows.push_back(is_shown);
    side_.setTabVisible(tab, is_shown);
    shown += is_shown ? 1 : 0;
  }
  // One panel needs no tab to pick it.
  side_.tabBar()->setVisible(shown > 1);
  const bool is_current_hidden = !shows[static_cast<size_t>(
      side_.currentIndex())];
  if (is_current_hidden) {
    for (int tab = 0; tab < side_.count(); ++tab) {
      const bool is_shown = shows[static_cast<size_t>(tab)];
      if (is_shown) {
        side_.setCurrentIndex(tab);
        break;
      }
    }
  }
  const bool is_sound = panels.sound;
  const bool is_shot = panels.shot;
  if (is_sound) {
    sound_panel_.Refresh();
  }
  if (is_shot) {
    shot_panel_.Refresh();
  }
}

}  // namespace snapper
