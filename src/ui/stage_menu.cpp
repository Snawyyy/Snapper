// StageView's right-click menu: linking movement.

#include <QContextMenuEvent>

#include <cassert>
#include <vector>

#include "edit/history_manager.h"
#include "edit/link_manager.h"
#include "edit/selection_manager.h"
#include "render/stage_hit.h"
#include "ui/problem.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

// Greys action out with why_not as its tooltip, or enables it.
void Explain(QAction* action, const QString& why_not) {
  assert(action != nullptr);
  assert(why_not.size() < 100000);
  action->setEnabled(why_not.isEmpty());
  action->setToolTip(why_not);
}

std::vector<LinkEnd> EndsOf(const std::set<Pick>& picks) {
  assert(picks.size() < 100000);
  std::vector<LinkEnd> ends;
  for (const Pick& pick : picks) {
    ends.push_back(LinkEnd{pick.layer, pick.piece});
  }
  return ends;
}

}  // namespace

void StageView::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  const auto frame = CurrentFrame();
  const bool is_usable = frame.has_value() && !tool_.IsDragging();
  if (!is_usable) {
    return;
  }
  QMenu menu(this);
  menu.setToolTipsVisible(true);
  AddLinkActions(&menu, event->pos(), *frame);
  const bool has_actions = !menu.isEmpty();
  if (has_actions) {
    menu.exec(event->globalPos());
  }
}

void StageView::AddLinkActions(QMenu* menu, QPointF point,
                               const StageFrame& frame) {
  assert(menu != nullptr);
  assert(managers_.links != nullptr);
  const Project& project = managers_.history->current();
  const Shot* shot = FindShot(project, frame.shot);
  if (shot == nullptr) {
    return;
  }
  const std::vector<LinkEnd> picked =
      managers_.selection->shot() == frame.shot
          ? EndsOf(managers_.selection->picks())
          : std::vector<LinkEnd>();
  const auto hit = HitTest(project, *shot, frame.local, point - frame.corner,
                           frame.scale, renderer_.cache());
  std::vector<LinkEnd> leaders;
  if (hit) {
    leaders.push_back(LinkEnd{hit->layer, QString()});
    const bool is_piece = !hit->piece.isEmpty();
    if (is_piece) {
      leaders.push_back(LinkEnd{hit->layer, hit->piece});
    }
  }
  LinkManager* links = managers_.links;
  for (const LinkEnd& leader : leaders) {
    QAction* link = menu->addAction(
        tr("Link movement to %1").arg(links->NameOf(frame.shot, leader)));
    link->setToolTip(tr("The picked things copy its movement from this "
                        "frame on, from where they are."));
    const QString why_not = links->WhyNoLink(frame.shot, picked, leader);
    Explain(link, why_not);
    // A tooltip that explains is better than one that repeats the label.
    if (!why_not.isEmpty()) {
      link->setToolTip(why_not);
    }
    connect(link, &QAction::triggered, this,
            [this, shot = frame.shot, picked, leader, from = frame.local] {
              emit Problem(ProblemOf(
                  managers_.links->LinkTo(shot, picked, leader, from)));
            });
  }
  const QString no_unlink = links->WhyNoUnlink(frame.shot, picked);
  const bool can_unlink = no_unlink.isEmpty();
  if (can_unlink) {
    QAction* unlink = menu->addAction(tr("Unlink movement"));
    connect(unlink, &QAction::triggered, this,
            [this, shot = frame.shot, picked] {
              emit Problem(
                  ProblemOf(managers_.links->Unlink(shot, picked)));
            });
  }
}

}  // namespace snapper
