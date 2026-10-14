#include "ui/comp_effects.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QTimer>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"
#include "ui/comp_drag.h"
#include "ui/comp_panel.h"
#include "ui/form_helpers.h"
#include "ui/on_switch.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The margin inside an open fold, past the indent on the left.
constexpr int kBodyMargin = 4;
constexpr int kMark = 12;

// An effect's raised plate, with a Teto red edge while it is picked.
class PlateFrame final : public QFrame {
 public:
  explicit PlateFrame(QWidget* parent) : QFrame(parent) {
    assert(parent != nullptr);
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setCursor(Qt::PointingHandCursor);
    assert(frameStyle() != 0);
  }

  void SetPicked(bool is_picked) {
    const bool is_same = is_picked_ == is_picked;
    if (!is_same) {
      is_picked_ = is_picked;
      update();
    }
  }

 protected:
  void paintEvent(QPaintEvent* event) override {
    assert(event != nullptr);
    QFrame::paintEvent(event);
    if (is_picked_) {
      QPainter painter(this);
      painter.setPen(QPen(theme::kPick, 2.0));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(QRectF(rect()).adjusted(1, 1, -1, -1));
    }
  }

 private:
  bool is_picked_ = false;
};

}  // namespace

CompEffects::CompEffects(const Managers& managers)
    : managers_(managers), layout_(this), light_layout_(&light_body_),
      light_(managers), drop_line_(this) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  setAcceptDrops(true);
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(theme::kRowGap);
  title_.setText(tr("Screen"));
  MakeTitlePlate(&title_);
  layout_.addWidget(&title_);
  folds_ = MakeOwned<QWidget>(this);
  layout_.addWidget(folds_);
  light_layout_.setContentsMargins(0, 0, 0, 0);
  light_layout_.addWidget(&light_);
  layout_.addWidget(PlatedSection(tr("Shot light"), &light_body_, this));
  layout_.addStretch();
  drop_line_.setAutoFillBackground(true);
  QPalette red = drop_line_.palette();
  red.setColor(QPalette::Window, theme::kPick);
  drop_line_.setPalette(red);
  drop_line_.hide();
  connect(&light_, &CompLight::Problem, this, &CompEffects::Problem);
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.comp, &CompManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  Refresh();
}

std::vector<CompEffect> CompEffects::Shown() const {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  return managers_.comp->ShownFor(managers_.selection->shot(),
                                  managers_.selection->layer(),
                                  managers_.selection->pieces());
}

CompTarget CompEffects::PickedTarget() const {
  assert(managers_.selection != nullptr);
  assert(managers_.comp != nullptr);
  CompTarget picked;
  picked.layer = managers_.selection->layer();
  for (const QString& piece : managers_.selection->pieces()) {
    picked.pieces.push_back(piece);
  }
  return picked;
}

QString CompEffects::Signature(const std::vector<CompEffect>& shown) const {
  assert(managers_.selection != nullptr);
  assert(shown.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  QString signature = QStringLiteral("%1/%2")
                          .arg(managers_.selection->shot().value())
                          .arg(managers_.selection->layer().value());
  for (const QString& piece : managers_.selection->pieces()) {
    signature += QStringLiteral("/") + piece;
  }
  for (const CompEffect& effect : shown) {
    signature += QStringLiteral("e%1").arg(effect.id.value());
  }
  return signature;
}

void CompEffects::Refresh() {
  assert(managers_.comp != nullptr);
  assert(managers_.selection != nullptr);
  const ShotId shot = managers_.selection->shot();
  const std::vector<CompEffect> shown = Shown();
  const QString where = managers_.comp->TargetText(shot, PickedTarget());
  const bool has_any = !shown.empty();
  title_.setText(has_any ? tr("%1  (%2)").arg(where).arg(shown.size())
                         : where);
  const QString signature = Signature(shown);
  const bool is_new = signature != signature_;
  if (!is_new) {
    ShowPlates();
    return;
  }
  std::set<int> had;
  for (const Plate& plate : plates_) {
    had.insert(plate.id.value());
  }
  signature_ = signature;
  Rebuild(shown);
  // Keys act on the picked effect, so it must be one in view.
  const bool is_picked_shown =
      CompEffectIndex(shown, managers_.comp->PickedIn(shot)) >= 0;
  if (!is_picked_shown) {
    managers_.comp->Pick(shot, CompEffectId());
  }
  ShowPlates();
  // One new effect, just added or pasted, comes into view.
  QFrame* fresh = nullptr;
  int fresh_count = 0;
  for (const Plate& plate : plates_) {
    const bool is_fresh = !had.contains(plate.id.value());
    fresh_count += is_fresh ? 1 : 0;
    fresh = is_fresh ? plate.body : fresh;
  }
  const bool is_one_new = fresh_count == 1;
  if (is_one_new) {
    QTimer::singleShot(0, fresh, [this, fresh] { emit Reveal(fresh); });
  }
}

void CompEffects::Rebuild(const std::vector<CompEffect>& shown) {
  assert(folds_ != nullptr);
  assert(shown.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  plates_.clear();
  plates_.reserve(shown.size());
  const int at = layout_.indexOf(folds_);
  folds_->hide();
  folds_->deleteLater();
  folds_ = MakeOwned<QWidget>(this);
  layout_.insertWidget(at, folds_);
  auto* column = MakeOwned<QVBoxLayout>(folds_);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap);
  const bool is_empty = shown.empty();
  if (is_empty) {
    column->addWidget(EmptyHint(
        tr("No effects here yet. Double click one on the left, or drag it "
           "here or onto a doll on the stage."),
        folds_));
  }
  for (const CompEffect& effect : shown) {
    AddFold(effect, column);
  }
}

QFrame* CompEffects::NewPlate(const CompEffect& effect, Plate* plate) {
  assert(plate != nullptr);
  assert(folds_ != nullptr);
  const ShotId shot = managers_.selection->shot();
  auto* frame = MakeOwned<PlateFrame>(folds_);
  frame->setToolTip(QStringLiteral("%1\n%2").arg(
      CompManager::EffectHelp(effect),
      tr("Works on %1. Click to pick and open or close it, drag to "
         "reorder, right click for more.")
          .arg(managers_.comp->TargetText(shot, effect.target))));
  auto* row = MakeOwned<QHBoxLayout>(frame);
  row->setContentsMargins(4, 2, 2, 2);
  row->setSpacing(6);
  auto* mark = MakeOwned<QLabel>(frame);
  mark->setPixmap(MarkIcon(effect.kind).pixmap(kMark, kMark));
  plate->eye = MakeOwned<OnSwitch>(frame);
  plate->eye->setToolTip(tr("Turn the effect on or off."));
  plate->name = MakeOwned<QLabel>(CompManager::EffectName(effect), frame);
  QFont bold = plate->name->font();
  bold.setBold(true);
  plate->name->setFont(bold);
  plate->name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  plate->slow = MakeOwned<QLabel>(tr("slow"), frame);
  plate->slow->setStyleSheet(QStringLiteral("QLabel { color: #f2c230; }"));
  plate->slow->setToolTip(tr("It took longer than a frame at 24 fps."));
  plate->arrow = MakeOwned<QToolButton>(frame);
  plate->arrow->setAutoRaise(true);
  plate->arrow->setFocusPolicy(Qt::NoFocus);
  plate->arrow->setToolTip(tr("Show or hide its settings."));
  row->addWidget(plate->eye);
  row->addWidget(mark);
  row->addWidget(plate->name, 1);
  row->addWidget(plate->slow);
  row->addWidget(plate->arrow);
  frame->installEventFilter(this);
  plate->name->installEventFilter(this);
  return frame;
}

void CompEffects::AddFold(const CompEffect& effect, QVBoxLayout* into) {
  assert(into != nullptr);
  assert(effect.id.IsValid());
  const CompEffectId id = effect.id;
  Plate plate;
  plate.id = id;
  plate.plate = NewPlate(effect, &plate);
  plate.plate->setContextMenuPolicy(Qt::CustomContextMenu);
  into->addWidget(plate.plate);
  plate.body = NewBody(id);
  into->addWidget(plate.body);
  const std::pair<int, int> key{managers_.selection->shot().value(),
                                id.value()};
  const bool is_open = !closed_.contains(key);
  plate.body->setVisible(is_open);
  plate.arrow->setArrowType(is_open ? Qt::DownArrow : Qt::LeftArrow);
  plates_.push_back(plate);
  connect(plate.arrow, &QToolButton::clicked, this, [this, id] {
    for (const Plate& one : plates_) {
      const bool is_it = one.id == id;
      if (is_it) {
        Toggle(one);
      }
    }
  });
  connect(plate.eye, &QAbstractButton::clicked, this, [this, id](bool is_on) {
    Report(ProblemOf(
        managers_.comp->SetOn(managers_.selection->shot(), id, is_on)));
  });
  QFrame* frame = plate.plate;
  connect(frame, &QWidget::customContextMenuRequested, this,
          [this, id, frame](const QPoint& at) {
            OpenMenu(id, frame->mapToGlobal(at));
          });
}

QFrame* CompEffects::NewBody(CompEffectId id) {
  assert(id.IsValid());
  assert(folds_ != nullptr);
  auto* body = MakeOwned<QFrame>(folds_);
  body->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  auto* inside = MakeOwned<QVBoxLayout>(body);
  inside->setContentsMargins(theme::kIndent, kBodyMargin, kBodyMargin,
                             kBodyMargin);
  auto* panel = MakeOwned<CompPanel>(managers_, id, body);
  inside->addWidget(panel);
  connect(panel, &CompPanel::Problem, this, &CompEffects::Problem);
  return body;
}

void CompEffects::Toggle(const Plate& plate) {
  assert(plate.body != nullptr);
  assert(plate.arrow != nullptr);
  const std::pair<int, int> key{managers_.selection->shot().value(),
                                plate.id.value()};
  const bool was_open = plate.body->isVisible();
  plate.body->setVisible(!was_open);
  plate.arrow->setArrowType(was_open ? Qt::LeftArrow : Qt::DownArrow);
  if (was_open) {
    closed_.insert(key);
  } else {
    closed_.erase(key);
  }
}

void CompEffects::ShowPlates() {
  assert(managers_.comp != nullptr);
  assert(plates_.size() <= static_cast<size_t>(kMaxCompEffectsPerShot));
  const ShotId shot = managers_.selection->shot();
  const CompEffectId picked = managers_.comp->PickedIn(shot);
  for (const Plate& plate : plates_) {
    const CompEffect* effect = managers_.comp->Find(shot, plate.id);
    const bool is_gone = effect == nullptr;
    if (is_gone) {
      continue;
    }
    plate.eye->setChecked(effect->is_on);
    static_cast<PlateFrame*>(plate.plate)->SetPicked(plate.id == picked);
    QPalette quiet = plate.name->palette();
    quiet.setColor(QPalette::WindowText,
                   effect->is_on ? palette().color(QPalette::WindowText)
                                 : theme::kTextOff);
    plate.name->setPalette(quiet);
    plate.slow->setVisible(managers_.comp->IsSlow(plate.id));
  }
}

}  // namespace snapper
