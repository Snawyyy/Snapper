#include "ui/dial_box.h"

#include <QInputDialog>
#include <QMenu>

#include <algorithm>
#include <cassert>

#include "anim/smart_dial.h"
#include "edit/dial_manager.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"
#include "edit/selection_manager.h"
#include "ui/follow.h"
#include "ui/form_helpers.h"
#include "ui/playhead.h"
#include "ui/problem.h"

namespace snapper {

DialBox::DialBox(const Managers& managers)
    : QGroupBox(tr("Smart dials")), managers_(managers), layout_(this) {
  assert(managers_.IsComplete());
  assert(managers_.dials != nullptr);
  for (int kind = 0; kind < kDialTemplateCount; ++kind) {
    auto* tile = new QPushButton(
        tr("+ %1").arg(TemplateName(static_cast<DialTemplate>(kind))), this);
    tiles_.addWidget(tile);
    tile_buttons_.push_back(tile);
    connect(tile, &QPushButton::clicked, this,
            [this, kind] { AddDial(kind); });
  }
  layout_.addLayout(&tiles_);
  layout_.addLayout(&rows_layout_);
  empty_.setWordWrap(true);
  empty_.setText(tr("Pick the pieces a dial should move, then a tile."));
  layout_.addWidget(&empty_);
  Follow(managers_, this);
  Refresh();
  assert(tile_buttons_.size() == static_cast<size_t>(kDialTemplateCount));
}

std::vector<QString> DialBox::PickedPieces() const {
  assert(managers_.selection != nullptr);
  const LayerId focus = managers_.selection->layer();
  std::vector<QString> pieces;
  for (const Pick& pick : managers_.selection->picks()) {
    if (pick.layer == focus && !pick.piece.isEmpty()) {
      pieces.push_back(pick.piece);
    }
  }
  return pieces;
}

QString DialBox::DollName() const {
  const Doll* doll =
      DollOfLayer(managers_.history->current(), managers_.selection->shot(),
                  managers_.selection->layer());
  return doll != nullptr ? doll->name : QString();
}

void DialBox::Report(const QString& problem) {
  assert(problem.size() < 100000);
  emit Problem(problem);
}

void DialBox::Refresh() {
  assert(managers_.selection != nullptr);
  const bool is_dragging = scope_ != nullptr;
  if (is_dragging) {
    return;
  }
  const ShotId shot = managers_.selection->shot();
  const LayerId layer = managers_.selection->layer();
  const std::vector<QString> pieces = PickedPieces();
  const QString why = managers_.dials->WhyNoDial(shot, layer, pieces);
  for (QPushButton* tile : tile_buttons_) {
    Explain(tile, why);
  }
  // Picking pieces only shows the dials that move one of them.
  std::vector<SmartDial> dials;
  for (const SmartDial& dial : managers_.dials->DialsOf(shot, layer, {})) {
    const bool is_shown =
        pieces.empty() || std::ranges::any_of(pieces, [&dial](const auto& p) {
          return std::ranges::find(dial.pieces, p) != dial.pieces.end();
        });
    if (is_shown) {
      dials.push_back(dial);
    }
  }
  const bool is_new = dials != shown_;
  if (is_new) {
    Rebuild(dials);
  }
  empty_.setVisible(rows_.empty());
  const auto spot = SpotOf(managers_);
  const bool is_here = spot.has_value() && spot->shot == shot;
  for (size_t i = 0; i < rows_.size(); ++i) {
    const SmartDial& dial = shown_[i];
    const double value = is_here ? managers_.dials->ValueOf(
                                       shot, layer, spot->local, dial.name)
                                 : RestValue(dial);
    rows_[i].track->Show(dial, value);
    rows_[i].readout->setText(DialText(dial, value));
    Explain(rows_[i].set_stop,
            is_here ? QString() : tr("Move the playhead into this shot."));
    rows_[i].track->setEnabled(is_here);
  }
}

void DialBox::Rebuild(const std::vector<SmartDial>& dials) {
  assert(dials.size() <= static_cast<size_t>(kMaxSmartDials));
  for (Row& row : rows_) {
    rows_layout_.removeWidget(row.widget);
    row.widget->deleteLater();
  }
  rows_.clear();
  shown_ = dials;
  for (const SmartDial& dial : dials) {
    Row row;
    row.name = dial.name;
    row.widget = new QWidget(this);
    auto* lines = new QVBoxLayout(row.widget);
    lines->setContentsMargins(0, 2, 0, 2);
    auto* top = new QHBoxLayout();
    row.title = new QLabel(QStringLiteral("<b>%1</b>")
                               .arg(dial.name.toHtmlEscaped()),
                           row.widget);
    row.readout = new QLabel(row.widget);
    row.set_stop = new QPushButton(tr("Set stop"), row.widget);
    row.set_stop->setToolTip(
        tr("Save how its pieces look now as the stop under the knob"));
    top->addWidget(row.title);
    top->addStretch(1);
    top->addWidget(row.readout);
    top->addWidget(row.set_stop);
    lines->addLayout(top);
    row.track = new DialTrack(row.widget);
    lines->addWidget(row.track);
    rows_layout_.addWidget(row.widget);
    const QString name = dial.name;
    row.widget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(row.widget, &QWidget::customContextMenuRequested, this,
            [this, name, widget = row.widget](const QPoint& at) {
              ShowMenu(name, -1, widget->mapToGlobal(at));
            });
    connect(row.set_stop, &QPushButton::clicked, this,
            [this, name] { SetStop(name); });
    connect(row.track, &DialTrack::Started, this, [this, name] {
      scope_ = std::make_unique<EditScope>(managers_.history,
                                           tr("%1 dial").arg(name));
    });
    connect(row.track, &DialTrack::Turned, this,
            [this, name](double value) { Turn(name, value); });
    connect(row.track, &DialTrack::Landed, this, [this] {
      scope_.reset();
      Refresh();
    });
    connect(row.track, &DialTrack::Cancelled, this, [this] {
      if (scope_ != nullptr) {
        scope_->Cancel();
      }
      scope_.reset();
      Refresh();
    });
    connect(row.track, &DialTrack::Rested, this, [this, name] {
      const auto found = std::ranges::find(shown_, name, &SmartDial::name);
      if (found != shown_.end()) {
        Turn(name, RestValue(*found));
      }
    });
    connect(row.track, &DialTrack::MenuAsked, this,
            [this, name](int stop, QPoint global) {
              ShowMenu(name, stop, global);
            });
    rows_.push_back(row);
  }
  assert(rows_.size() == shown_.size());
}

void DialBox::AddDial(int kind) {
  assert(kind >= 0 && kind < kDialTemplateCount);
  const auto added = managers_.dials->AddDial(
      DollName(), static_cast<DialTemplate>(kind), PickedPieces());
  Report(ProblemOf(added));
}

void DialBox::Turn(const QString& dial, double value) {
  const auto spot = SpotOf(managers_);
  const bool is_here =
      spot.has_value() && spot->shot == managers_.selection->shot();
  if (!is_here) {
    return;
  }
  const auto turned =
      managers_.dials->Turn(spot->shot, managers_.selection->PickedLayers(),
                            spot->local, dial, value);
  Report(ProblemOf(turned));
  const auto row = std::ranges::find(rows_, dial, &Row::name);
  const auto found = std::ranges::find(shown_, dial, &SmartDial::name);
  if (row != rows_.end() && found != shown_.end()) {
    row->readout->setText(DialText(*found, value));
  }
}

void DialBox::SetStop(const QString& dial) {
  const auto spot = SpotOf(managers_);
  const bool is_here =
      spot.has_value() && spot->shot == managers_.selection->shot();
  if (!is_here) {
    return;
  }
  const LayerId layer = managers_.selection->layer();
  const double value =
      managers_.dials->ValueOf(spot->shot, layer, spot->local, dial);
  Report(ProblemOf(managers_.dials->SetStop(spot->shot, layer, spot->local,
                                            dial, value, QString())));
}

void DialBox::ShowMenu(const QString& dial, int stop, QPoint global) {
  const QString doll = DollName();
  const auto found = std::ranges::find(shown_, dial, &SmartDial::name);
  if (doll.isEmpty() || found == shown_.end()) {
    return;
  }
  DialManager* dials = managers_.dials;
  QMenu menu(this);
  const bool is_stop = stop >= 0;
  if (is_stop) {
    const QString stop_name = found->stops[static_cast<size_t>(stop)].name;
    menu.addSection(stop_name);
    menu.addAction(tr("Rename stop..."), this, [=, this] {
      bool is_ok = false;
      const QString to = QInputDialog::getText(
          this, tr("Rename stop"), tr("Name"), QLineEdit::Normal, stop_name,
          &is_ok);
      if (is_ok) {
        Report(ProblemOf(dials->RenameStop(doll, dial, stop, to)));
      }
    });
    menu.addAction(tr("Make it the rest stop"), this, [=, this] {
      Report(ProblemOf(dials->SetRest(doll, dial, stop)));
    });
    menu.addAction(tr("Move it to the knob"), this, [=, this] {
      const auto row = std::ranges::find(rows_, dial, &Row::name);
      if (row != rows_.end()) {
        Report(ProblemOf(
            dials->MoveStop(doll, dial, stop, row->track->value())));
      }
    });
    menu.addAction(tr("Delete stop"), this, [=, this] {
      Report(ProblemOf(dials->RemoveStop(doll, dial, stop)));
    });
  }
  menu.addSection(dial);
  menu.addAction(tr("Rename dial..."), this, [=, this] {
    bool is_ok = false;
    const QString to = QInputDialog::getText(
        this, tr("Rename dial"), tr("Name"), QLineEdit::Normal, dial, &is_ok);
    if (is_ok) {
      Report(ProblemOf(dials->RenameDial(doll, dial, to)));
    }
  });
  QAction* drive = menu.addAction(tr("Drive the picked pieces"), this,
                                  [=, this] {
                                    Report(ProblemOf(dials->SetPieces(
                                        doll, dial, PickedPieces())));
                                  });
  Explain(drive, PickedPieces().empty() ? tr("Pick some pieces first.")
                                        : QString());
  QAction* halfway = menu.addAction(tr("Swap drawings halfway"), this,
                                    [=, this] {
                                      Report(ProblemOf(dials->SetSwap(
                                          doll, dial, DialSwap::kHalfway)));
                                    });
  halfway->setCheckable(true);
  halfway->setChecked(found->swap == DialSwap::kHalfway);
  QAction* at_stop = menu.addAction(tr("Swap drawings at the stop"), this,
                                    [=, this] {
                                      Report(ProblemOf(dials->SetSwap(
                                          doll, dial, DialSwap::kAtStop)));
                                    });
  at_stop->setCheckable(true);
  at_stop->setChecked(found->swap == DialSwap::kAtStop);
  menu.addAction(tr("Move up"), this, [=, this] {
    Report(ProblemOf(dials->MoveDial(doll, dial, -1)));
  });
  menu.addAction(tr("Move down"), this, [=, this] {
    Report(ProblemOf(dials->MoveDial(doll, dial, 1)));
  });
  menu.addAction(tr("Delete dial"), this, [=, this] {
    Report(ProblemOf(dials->RemoveDial(doll, dial)));
  });
  menu.exec(global);
}

}  // namespace snapper
