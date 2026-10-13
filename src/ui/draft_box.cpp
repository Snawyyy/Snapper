#include "ui/draft_box.h"

#include <QFormLayout>

#include <cassert>

#include "edit/draft_manager.h"
#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/draw_actions.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/pose_controls.h"
#include "ui/problem.h"
#include "ui/toast_manager.h"

namespace snapper {

DraftBox::DraftBox(const Managers& managers)
    : QGroupBox(tr("Draft")), managers_(managers), live_(managers.history),
      layout_(this), shown_(tr("Shown")), draw_new_(tr("New cel here")),
      draw_over_(tr("Draw over")), remove_(tr("Delete picked cels")) {
  assert(managers_.IsComplete());
  assert(managers_.draft != nullptr);
  QFormLayout* form = SettingsForm(&form_host_);
  layout_.addWidget(&form_host_);
  shown_.setToolTip(tr("The draft's eye; its row on the timeline has one "
                       "too."));
  form->addRow(&shown_);
  SetUpNumber(&opacity_, Number::kFraction);
  opacity_.setToolTip(tr("How strongly the draft shows over the shot."));
  form->addRow(MakeFaderRow(tr("Opacity"), &opacity_, &live_, 1.0,
                            kDefaultDraftOpacity, &form_host_));
  form->addRow(TileRow({&draw_new_, &draw_over_}, &form_host_));
  form->addRow(&remove_);
  count_.setWordWrap(true);
  form->addRow(&count_);
  DraftManager* draft = managers_.draft;
  connect(&shown_, &QCheckBox::clicked, this, [this, draft](bool is_on) {
    Report(ProblemOf(draft->SetShown(Shot(), is_on)));
  });
  MakeLive(&opacity_, &live_, tr("Draft opacity"), this, [this, draft] {
    Report(ProblemOf(draft->SetOpacity(Shot(), opacity_.value())));
  });
  connect(&draw_new_, &QPushButton::clicked, this,
          [this] { RequestDraft(managers_, false); });
  connect(&draw_over_, &QPushButton::clicked, this,
          [this] { RequestDraft(managers_, true); });
  connect(&remove_, &QPushButton::clicked, this, [this, draft] {
    Report(ProblemOf(draft->DeleteCels(Shot(), draft->Picked(Shot()))));
  });
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, refresh);
  connect(draft, &DraftManager::Changed, this, refresh);
  if (managers_.draw != nullptr) {
    connect(managers_.draw, &DrawManager::Started, this, refresh);
    connect(managers_.draw, &DrawManager::Ended, this, refresh);
  }
  Refresh();
}

ShotId DraftBox::Shot() const {
  assert(managers_.selection != nullptr);
  return managers_.selection->shot();
}

void DraftBox::Report(const QString& problem) const {
  assert(managers_.toasts != nullptr);
  if (!problem.isEmpty()) {
    managers_.toasts->Problem(problem);
  }
}

void DraftBox::Refresh() {
  assert(managers_.draft != nullptr);
  const Draft* draft = managers_.draft->Of(Shot());
  const QString no_shot = draft == nullptr ? tr("Pick a shot first.")
                                           : QString();
  Explain(&shown_, no_shot,
          tr("The draft's eye; its row on the timeline has one too."));
  Explain(&opacity_, no_shot, tr("How strongly the draft shows over the "
                                 "shot."));
  Explain(&draw_new_, WhyNoDraftHere(managers_, false),
          tr("A blank cel from the playhead on, over the shot, with the "
             "cels around it as onion skins (Alt+N)."));
  Explain(&draw_over_, WhyNoDraftHere(managers_, true),
          tr("Draw on the cel showing at the playhead (Alt+Shift+N)."));
  Explain(&remove_, managers_.draft->WhyNoDelete(Shot()),
          tr("Delete the cels picked on the Draft row (Delete)."));
  if (draft == nullptr) {
    count_.setText(QString());
    return;
  }
  const QSignalBlocker quiet_eye(shown_);
  shown_.setChecked(draft->is_shown);
  ShowNumber(&opacity_, draft->opacity);
  const size_t count = draft->cels.size();
  const QString cels = count == 1 ? tr("1 cel.")
                                  : tr("%1 cels.").arg(count);
  count_.setText(cels + QLatin1Char(' ') +
                 tr("Seen on the Pose stage and the Video preview, never "
                    "in the exported video."));
}

}  // namespace snapper
