#include "ui/storyboard_box.h"

#include <QFormLayout>

#include <cassert>
#include <cmath>

#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/storyboard_manager.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/pose_controls.h"
#include "ui/problem.h"
#include "ui/storyboard_actions.h"

namespace snapper {

StoryboardBox::StoryboardBox(const Managers& managers)
    : QGroupBox(tr("Storyboard")),
      managers_(managers),
      live_(managers.history),
      layout_(this),
      exported_(tr("In export")),
      draw_new_(tr("New frame here")),
      draw_over_(tr("Draw over")),
      remove_(tr("Delete picked frames")) {
  assert(managers_.IsComplete());
  assert(managers_.storyboard != nullptr);
  QFormLayout* form = SettingsForm(&form_host_);
  layout_.addWidget(&form_host_);
  SetUpNumber(&opacity_, Number::kFraction);
  form->addRow(MakeFaderRow(tr("Opacity"), &opacity_, &live_, 1.0,
                            kDefaultStoryboardOpacity, &form_host_));
  form->addRow(&exported_);
  form->addRow(TileRow({&draw_new_, &draw_over_}, &form_host_));
  form->addRow(&remove_);
  count_.setWordWrap(true);
  form->addRow(&count_);
  StoryboardManager* board = managers_.storyboard;
  connect(&exported_, &QCheckBox::clicked, this, [this, board](bool is_on) {
    emit Problem(ProblemOf(board->SetExported(Shot(), Layer(), is_on)));
  });
  MakeLive(&opacity_, &live_, tr("Storyboard opacity"), this, [this, board] {
    emit Problem(
        ProblemOf(board->SetOpacity(Shot(), Layer(), opacity_.value())));
  });
  connect(&draw_new_, &QPushButton::clicked, this,
          [this] { RequestStoryboard(managers_, false); });
  connect(&draw_over_, &QPushButton::clicked, this,
          [this] { RequestStoryboard(managers_, true); });
  connect(&remove_, &QPushButton::clicked, this, [this, board] {
    emit Problem(ProblemOf(board->DeleteCels(
        Shot(), Layer(), board->Picked(Shot(), Layer()))));
  });
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, refresh);
  connect(board, &StoryboardManager::Changed, this, refresh);
  connect(managers_.draw, &DrawManager::Started, this, refresh);
  connect(managers_.draw, &DrawManager::Ended, this, refresh);
  Refresh();
}

ShotId StoryboardBox::Shot() const {
  assert(managers_.selection != nullptr);
  assert(managers_.storyboard != nullptr);
  return managers_.selection->shot();
}

LayerId StoryboardBox::Layer() const {
  assert(managers_.storyboard != nullptr);
  assert(managers_.selection != nullptr);
  return managers_.storyboard->PickedBoard(Shot());
}

void StoryboardBox::Refresh() {
  assert(managers_.storyboard != nullptr);
  assert(managers_.history != nullptr);
  const StoryboardLayer* board = managers_.storyboard->Of(Shot(), Layer());
  const bool has_board = board != nullptr;
  const QString no_layer =
      has_board ? QString() : tr("Pick a storyboard layer first.");
  Explain(&exported_, no_layer,
          tr("Put this storyboard in the exported video, for an "
             "animatic."));
  Explain(&opacity_, no_layer,
          tr("How strongly the storyboard shows over what lies under it."));
  Explain(&draw_new_, WhyNoStoryboardHere(managers_, false),
          tr("A blank frame from the playhead on, with the frame before "
             "faint to trace from (Alt+N)."));
  Explain(&draw_over_, WhyNoStoryboardHere(managers_, true),
          tr("Draw on the frame showing at the playhead (Alt+Shift+N)."));
  Explain(&remove_, managers_.storyboard->WhyNoDelete(Shot()),
          tr("Delete the frames picked on the storyboard row (Delete)."));
  if (!has_board) {
    count_.setText(QString());
    return;
  }
  const QSignalBlocker quiet_export(exported_);
  exported_.setChecked(board->is_exported);
  ShowNumber(&opacity_, board->opacity);
  const size_t count = board->cels.size();
  const QString frames =
      count == 1 ? tr("1 frame.") : tr("%1 frames.").arg(count);
  count_.setText(frames + QLatin1Char(' ') +
                 tr("Seen on the Pose stage and the Video preview; in the "
                    "exported video only with In export on."));
}

}  // namespace snapper
