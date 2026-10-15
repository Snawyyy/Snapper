#include "ui/transport_bar.h"

#include <QFontMetrics>

#include <cassert>
#include <memory>

#include "base/frame.h"
#include "edit/playback_manager.h"
#include "ui/bar_text.h"
#include "ui/hover_fade.h"

namespace snapper {
namespace {

constexpr int kButtonPadPx = 12;

}  // namespace

TransportBar::TransportBar(const Managers& managers)
    : managers_(managers),
      layout_(this),
      first_(TapeGlyph::kFirst),
      back_(TapeGlyph::kBack),
      play_(TapeGlyph::kPlay),
      next_(TapeGlyph::kNext),
      last_(TapeGlyph::kLast),
      counter_(managers.motion),
      time_(managers.motion) {
  assert(managers_.IsComplete());
  PlaybackManager* playback = managers_.playback;
  assert(playback != nullptr);
  layout_.setContentsMargins(4, 2, 4, 2);
  layout_.setSpacing(2);
  // As wide as its buttons, so a bar beside others does not grow.
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  AddButton(&first_, tr("First frame"));
  AddButton(&back_, tr("Previous frame (Left)"));
  AddButton(&play_, tr("Play or pause (Space)"));
  AddButton(&next_, tr("Next frame (Right)"));
  AddButton(&last_, tr("Last frame"));
  play_.setCheckable(true);
  play_.setObjectName(QStringLiteral("play"));
  first_.setObjectName(QStringLiteral("first"));
  back_.setObjectName(QStringLiteral("back"));
  next_.setObjectName(QStringLiteral("next"));
  last_.setObjectName(QStringLiteral("last"));
  counter_.setObjectName(QStringLiteral("counter"));
  time_.setObjectName(QStringLiteral("time"));
  const QFontMetrics metrics(counter_.font());
  const int counter_wide =
      metrics.horizontalAdvance(QStringLiteral("F 9999"));
  for (BumpLabel* label : {&counter_, &time_}) {
    label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    label->setMargin(2);
    label->setFixedWidth(counter_wide + 2 * kButtonPadPx);
    label->setFixedHeight(label->sizeHint().height() + kBumpPx);
    layout_.addWidget(label);
  }
  connect(&first_, &QPushButton::clicked, playback,
          [playback] { playback->Seek(Frame(0)); });
  connect(&back_, &QPushButton::clicked, playback,
          [playback] { playback->Step(-1); });
  connect(&play_, &QPushButton::clicked, playback, &PlaybackManager::Toggle);
  connect(&next_, &QPushButton::clicked, playback,
          [playback] { playback->Step(1); });
  connect(&last_, &QPushButton::clicked, playback,
          [playback] { playback->Seek(Frame(kMaxFrame)); });
  connect(playback, &PlaybackManager::FrameChanged, this,
          &TransportBar::ShowPlayhead);
  connect(playback, &PlaybackManager::PlayingChanged, this,
          &TransportBar::ShowPlayhead);
  connect(playback, &PlaybackManager::TimelineChanged, this,
          &TransportBar::ShowPlayhead);
  ShowPlayhead();
}

void TransportBar::AddButton(QPushButton* button, const QString& tip) {
  assert(button != nullptr);
  assert(!tip.isEmpty());
  button->setFocusPolicy(Qt::NoFocus);
  button->setToolTip(tip);
  // The button owns it.
  std::make_unique<HoverFade>(button, managers_.motion).release();
  layout_.addWidget(button);
}

void TransportBar::ShowPlayhead() {
  assert(managers_.playback != nullptr);
  assert(managers_.history != nullptr);
  const Frame frame = managers_.playback->frame();
  const bool is_playing = managers_.playback->IsPlaying();
  // At 24 frames a second the hop would only blur, so only a step hops.
  counter_.SetText(FrameCounterText(frame), !is_playing);
  time_.setText(TimecodeText(frame));
  play_.setText(PlayText(is_playing));
  play_.setChecked(is_playing);
}

}  // namespace snapper
