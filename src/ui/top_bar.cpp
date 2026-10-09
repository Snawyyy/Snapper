#include "ui/top_bar.h"

#include <QFontMetrics>

#include <algorithm>
#include <cassert>
#include <memory>

#include "base/frame.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "ui/hover_fade.h"

namespace snapper {
namespace {

constexpr int kBarGap = 14;
constexpr int kStepMaxWidth = 190;
constexpr int kStepMinWidth = 64;
constexpr int kButtonPadPx = 12;

// Button text is read for & mnemonics; a step's name may hold one.
QString Plain(const QString& text) {
  assert(text.size() >= 0);
  assert(text.size() < 4096);
  QString plain = text;
  return plain.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

void ShowStepOn(QPushButton* button, const StepFace& face) {
  assert(button != nullptr);
  assert(!face.text.isEmpty());
  button->setEnabled(face.enabled);
  button->setText(Plain(face.text));
  // A name too long for the button is still readable in full.
  button->setToolTip(face.enabled ? face.text : face.tooltip);
}

}  // namespace

TopBar::TopBar(const Managers& managers)
    : managers_(managers),
      layout_(this),
      tabs_(managers.motion),
      first_(QStringLiteral("|<")),
      back_(QStringLiteral("<")),
      play_(PlayText(false)),
      next_(QStringLiteral(">")),
      last_(QStringLiteral(">|")),
      undo_(tr("Undo")),
      redo_(tr("Redo")) {
  assert(managers_.IsComplete());
  layout_.setContentsMargins(4, 4, 4, 2);
  layout_.setSpacing(2);
  tabs_.addTab(tr("Pose"));
  tabs_.addTab(tr("Rig"));
  tabs_.addTab(tr("Video"));
  tabs_.setObjectName(QStringLiteral("modes"));
  layout_.addWidget(&tabs_);
  layout_.addSpacing(kBarGap);
  BuildTransport();
  layout_.addSpacing(kBarGap);
  BuildSteps();
  layout_.addStretch();
  ShowPlayhead();
  assert(tabs_.count() == 3);
}

void TopBar::BuildTransport() {
  assert(managers_.playback != nullptr);
  assert(layout_.count() > 0);
  PlaybackManager* playback = managers_.playback;
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
  const QFontMetrics metrics(play_.font());
  const int wide = std::max(metrics.horizontalAdvance(PlayText(false)),
                            metrics.horizontalAdvance(PlayText(true)));
  play_.setFixedWidth(wide + 2 * kButtonPadPx);
  for (QPushButton* small : {&first_, &back_, &next_, &last_}) {
    small->setFixedWidth(metrics.horizontalAdvance(small->text()) +
                         2 * kButtonPadPx);
  }
  const int counter_wide =
      metrics.horizontalAdvance(QStringLiteral("F 9999"));
  for (QLabel* label : {&counter_, &time_}) {
    label->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    label->setMargin(2);
    label->setFixedWidth(counter_wide + 2 * kButtonPadPx);
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
          &TopBar::ShowPlayhead);
  connect(playback, &PlaybackManager::PlayingChanged, this,
          &TopBar::ShowPlayhead);
  connect(playback, &PlaybackManager::TimelineChanged, this,
          &TopBar::ShowPlayhead);
}

void TopBar::BuildSteps() {
  assert(managers_.history != nullptr);
  assert(layout_.count() > 0);
  undo_.setObjectName(QStringLiteral("undo"));
  redo_.setObjectName(QStringLiteral("redo"));
  AddButton(&undo_, tr("Nothing to undo"));
  AddButton(&redo_, tr("Nothing to redo"));
  for (QPushButton* step : {&undo_, &redo_}) {
    step->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    step->setMinimumWidth(kStepMinWidth);
    step->setMaximumWidth(kStepMaxWidth);
  }
  connect(&undo_, &QPushButton::clicked, managers_.history,
          &HistoryManager::Undo);
  connect(&redo_, &QPushButton::clicked, managers_.history,
          &HistoryManager::Redo);
}

void TopBar::AddButton(QPushButton* button, const QString& tip) {
  assert(button != nullptr);
  assert(!tip.isEmpty());
  button->setFocusPolicy(Qt::NoFocus);
  button->setToolTip(tip);
  // The button owns it.
  std::make_unique<HoverFade>(button, managers_.motion).release();
  layout_.addWidget(button);
}

void TopBar::ShowPlayhead() {
  assert(managers_.playback != nullptr);
  assert(managers_.history != nullptr);
  const Frame frame = managers_.playback->frame();
  const bool is_playing = managers_.playback->IsPlaying();
  counter_.setText(FrameCounterText(frame));
  time_.setText(TimecodeText(frame));
  play_.setText(PlayText(is_playing));
  play_.setChecked(is_playing);
}

void TopBar::ShowUndo(const StepFace& face) {
  assert(!face.text.isEmpty());
  assert(managers_.history != nullptr);
  ShowStepOn(&undo_, face);
}

void TopBar::ShowRedo(const StepFace& face) {
  assert(!face.text.isEmpty());
  assert(managers_.history != nullptr);
  ShowStepOn(&redo_, face);
}

}  // namespace snapper
