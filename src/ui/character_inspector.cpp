#include "ui/character_inspector.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QPainter>
#include <QVBoxLayout>

#include <cassert>
#include <cmath>

#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "ui/panel_kit.h"
#include "ui/theme.h"

namespace snapper {
namespace {

void SetUpNumber(QDoubleSpinBox* box, double low, double high, double step,
                 int decimals, const QString& suffix = QString()) {
  assert(box != nullptr);
  assert(low < high);
  box->setRange(low, high);
  box->setSingleStep(step);
  box->setDecimals(decimals);
  box->setSuffix(suffix);
  box->setKeyboardTracking(false);
}

}  // namespace

SwatchButton::SwatchButton(QWidget* parent) : QPushButton(parent) {
  setFixedSize(40, 24);
  setToolTip(tr("Pick a color"));
  connect(this, &QPushButton::clicked, this, [this] {
    const QColor picked = QColorDialog::getColor(
        color_, this, tr("Pick a color"), QColorDialog::ShowAlphaChannel);
    if (picked.isValid()) {
      SetColor(picked);
      emit Picked(picked);
    }
  });
}

void SwatchButton::SetColor(const QColor& color) {
  assert(color.isValid());
  assert(color_.isValid());
  color_ = color;
  update();
}

void SwatchButton::paintEvent(QPaintEvent*) {
  assert(color_.isValid());
  assert(width() > 0);
  QPainter painter(this);
  const QRect box = rect().adjusted(2, 2, -3, -3);
  painter.fillRect(box, Qt::white);
  painter.fillRect(QRect(box.topLeft(), box.size() / 2), QColor(200, 200, 200));
  painter.fillRect(QRect(box.center(), box.size() / 2), QColor(200, 200, 200));
  painter.fillRect(box, isEnabled() ? color_ : theme::kFaceDark);
  painter.setPen(hasFocus() ? theme::kPick : theme::kShadow);
  painter.drawRect(box);
}

CharacterInspector::CharacterInspector(HistoryManager* history,
                                       CharacterManager* characters,
                                       CharacterSession* session,
                                       QWidget* parent)
    : QWidget(parent),
      history_(history),
      characters_(characters),
      session_(session) {
  assert(history_ != nullptr && characters_ != nullptr);
  assert(session_ != nullptr);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(theme::kRowGap);
  layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
  part_section_ = PlatedSection(tr("Part"), BuildPart(), this);
  look_section_ = PlatedSection(tr("Look"), BuildLook(), this);
  bone_section_ = PlatedSection(tr("Bone"), BuildBone(), this);
  action_section_ = PlatedSection(tr("Smart bone action"), BuildAction(), this);
  // Each section keeps the height its rows ask for; the panel scrolls
  // instead of squeezing them.
  for (QWidget* section :
       {part_section_, look_section_, bone_section_, action_section_}) {
    section->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(section);
  }
  layout->addStretch(1);
  connect(history_, &HistoryManager::Changed, this, &CharacterInspector::Refresh);
  connect(session_, &CharacterSession::Changed, this,
          &CharacterInspector::Refresh);
  Refresh();
}

const Character* CharacterInspector::Shown() const {
  assert(history_ != nullptr);
  assert(session_ != nullptr);
  return session_->character().isEmpty()
             ? nullptr
             : FindCharacter(history_->current(), session_->character());
}

void CharacterInspector::Report(const Result<void>& done) {
  assert(session_ != nullptr);
  assert(history_ != nullptr);
  emit Problem(done ? QString() : done.error().message);
}

QWidget* CharacterInspector::BuildPart() {
  auto* body = new QWidget(this);
  QFormLayout* form = SettingsForm(body);
  part_form_ = form;
  SetUpNumber(&x_, -100000, 100000, 1, 1, tr(" px"));
  SetUpNumber(&y_, -100000, 100000, 1, 1, tr(" px"));
  SetUpNumber(&turn_, -3600, 3600, 1, 1, tr(" deg"));
  SetUpNumber(&scale_x_, -100, 100, 0.05, 2);
  SetUpNumber(&scale_y_, -100, 100, 0.05, 2);
  SetUpNumber(&opacity_, 0, 1, 0.05, 2);
  flipped_.setText(tr("Mirrored"));
  form->addRow(tr("Kind"), &part_kind_);
  form->addRow(tr("Across"), &x_);
  form->addRow(tr("Down"), &y_);
  form->addRow(tr("Turn"), &turn_);
  form->addRow(tr("Width"), &scale_x_);
  form->addRow(tr("Height"), &scale_y_);
  form->addRow(tr("Opacity"), &opacity_);
  form->addRow(QString(), &flipped_);
  bind_.addItem(tr("Bend with nearby bones"), static_cast<int>(BindMode::kFlexi));
  bind_.addItem(tr("Points tied by hand"), static_cast<int>(BindMode::kPoints));
  bind_.addItem(tr("Ride one bone"), static_cast<int>(BindMode::kLayer));
  bind_.addItem(tr("Not moved by bones"), static_cast<int>(BindMode::kNone));
  bind_.setToolTip(tr("How bones move this part"));
  form->addRow(tr("Bones"), &bind_);
  form->addRow(tr("Bone"), &bind_bone_);
  pick_label_.setText(tr("Shows"));
  form->addRow(&pick_label_, &rest_pick_);
  for (QDoubleSpinBox* box : {&x_, &y_, &turn_, &scale_x_, &scale_y_, &opacity_}) {
    connect(box, &QDoubleSpinBox::valueChanged, this, &CharacterInspector::CommitPart);
  }
  connect(&flipped_, &QCheckBox::toggled, this, &CharacterInspector::CommitPart);
  auto commit_bind = [this] {
    if (is_filling_) return;
    Report(characters_->SetBind(
        session_->character(), session_->part(),
        static_cast<BindMode>(bind_.currentData().toInt()),
        BoneId(bind_bone_.currentData().toInt())));
  };
  connect(&bind_, &QComboBox::activated, this, commit_bind);
  connect(&bind_bone_, &QComboBox::activated, this, commit_bind);
  connect(&rest_pick_, &QComboBox::activated, this, [this] {
    if (is_filling_) return;
    Report(characters_->SetRestPick(session_->character(), session_->part(),
                                    PartId(rest_pick_.currentData().toInt())));
  });
  return body;
}

QWidget* CharacterInspector::BuildLook() {
  auto* body = new QWidget(this);
  QFormLayout* form = SettingsForm(body);
  look_for_.setWordWrap(true);
  theme::SetLevel(&look_for_, theme::kWell);
  form->addRow(&look_for_);
  has_fill_.setText(tr("Fill"));
  has_stroke_.setText(tr("Line"));
  SetUpNumber(&stroke_width_, 0, kMaxStrokeWidth, 0.5, 1, tr(" px"));
  form->addRow(&has_fill_, &fill_);
  form->addRow(&has_stroke_, &stroke_);
  form->addRow(tr("Line width"), &stroke_width_);
  const char* const kBrushNames[kLineBrushCount] = {
      QT_TR_NOOP("Solid"),  QT_TR_NOOP("Pencil"), QT_TR_NOOP("Ink"),
      QT_TR_NOOP("Marker"), QT_TR_NOOP("Chalk"),  QT_TR_NOOP("Dots")};
  for (const char* name : kBrushNames) {
    brush_.addItem(tr(name));
  }
  brush_.setToolTip(tr("How the line is laid down: an even stroke, or "
                       "pencil grain, ink that swells mid-line, see-through "
                       "marker, broken chalk or dots"));
  form->addRow(tr("Brush"), &brush_);
  make_shape_.setText(tr("Make shape"));
  make_shape_.setToolTip(tr("Fill the picked points' closed lines (U)"));
  raise_.setText(tr("Forward"));
  lower_.setText(tr("Back"));
  form->addRow(TileRow({&make_shape_, &raise_, &lower_}, body));
  SetUpNumber(&curvature_, 0, kMaxCurvature, 0.05, 2);
  curvature_.setToolTip(tr("0 is a sharp corner, 0.33 an even round"));
  SetUpNumber(&point_width_, 0, 10, 0.1, 2);
  point_width_.setToolTip(tr("The line's width at these points, times the "
                             "shape's line width"));
  form->addRow(tr("Roundness"), &curvature_);
  form->addRow(tr("Taper"), &point_width_);
  weld_.setText(tr("Weld two points"));
  weld_.setToolTip(tr("Join the two picked points into one"));
  form->addRow(&weld_);
  for (QCheckBox* box : {&has_fill_, &has_stroke_}) {
    connect(box, &QCheckBox::toggled, this, &CharacterInspector::CommitLook);
  }
  connect(&fill_, &SwatchButton::Picked, this, &CharacterInspector::CommitLook);
  connect(&stroke_, &SwatchButton::Picked, this, &CharacterInspector::CommitLook);
  connect(&stroke_width_, &QDoubleSpinBox::valueChanged, this,
          &CharacterInspector::CommitLook);
  connect(&brush_, &QComboBox::activated, this,
          &CharacterInspector::CommitLook);
  connect(&make_shape_, &QPushButton::clicked, this, [this] {
    const std::vector<PointId> ids(session_->points().begin(),
                                   session_->points().end());
    const auto made = characters_->MakeShape(session_->character(),
                                             session_->part(), ids,
                                             session_->style());
    if (!made) {
      emit Problem(made.error().message);
      return;
    }
    session_->SetShape(*made);
    emit Problem(QString());
  });
  auto raise = [this](int by) {
    Report(characters_->RaiseShape(session_->character(), session_->part(),
                                   session_->shape(), by));
    const Character* character = Shown();
    const CharacterPart* part =
        character != nullptr ? FindPart(*character, session_->part()) : nullptr;
    if (part != nullptr) {
      const int last = static_cast<int>(part->drawing.shapes.size()) - 1;
      session_->SetShape(std::clamp(session_->shape() + by, 0, last));
    }
  };
  connect(&raise_, &QPushButton::clicked, this, [raise] { raise(1); });
  connect(&lower_, &QPushButton::clicked, this, [raise] { raise(-1); });
  connect(&curvature_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    if (is_filling_) return;
    const std::vector<PointId> ids(session_->points().begin(),
                                   session_->points().end());
    Report(characters_->SetCurvature(session_->character(), session_->part(),
                                     ids, value));
  });
  connect(&point_width_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
    if (is_filling_) return;
    const std::vector<PointId> ids(session_->points().begin(),
                                   session_->points().end());
    Report(characters_->SetWidth(session_->character(), session_->part(), ids,
                                 value));
  });
  connect(&weld_, &QPushButton::clicked, this, [this] {
    if (session_->points().size() != 2) return;
    const PointId keep = *session_->points().begin();
    const PointId drop = *session_->points().rbegin();
    Report(characters_->Weld(session_->character(), session_->part(), keep, drop));
    session_->SetPoints({keep});
  });
  return body;
}

QWidget* CharacterInspector::BuildBone() {
  auto* body = new QWidget(this);
  QFormLayout* form = SettingsForm(body);
  form->addRow(tr("Name"), &bone_name_);
  SetUpNumber(&strength_, 0, 100000, 1, 0, tr(" px"));
  strength_.setToolTip(tr("How far the bone pulls points (S drags it)"));
  form->addRow(tr("Reach"), &strength_);
  has_limits_.setText(tr("Limit the turn"));
  SetUpNumber(&min_turn_, -360, 360, 1, 0, tr(" deg"));
  SetUpNumber(&max_turn_, -360, 360, 1, 0, tr(" deg"));
  form->addRow(&has_limits_);
  form->addRow(tr("From"), &min_turn_);
  form->addRow(tr("To"), &max_turn_);
  locked_.setText(tr("Hold still in IK drags"));
  form->addRow(&locked_);
  SetUpNumber(&dynamics_, 0, 1, 0.05, 2);
  dynamics_.setToolTip(tr("How much the bone lags and bounces behind its "
                          "parent; 0 is off"));
  form->addRow(tr("Bounce"), &dynamics_);
  smart_.setText(tr("Smart bone"));
  smart_.setToolTip(tr("Its turn plays an action (a head turn, a blink) "
                       "instead of bending points"));
  form->addRow(&smart_);
  for (QDoubleSpinBox* box : {&strength_, &min_turn_, &max_turn_, &dynamics_}) {
    connect(box, &QDoubleSpinBox::valueChanged, this, &CharacterInspector::CommitBone);
  }
  for (QCheckBox* box : {&has_limits_, &locked_}) {
    connect(box, &QCheckBox::toggled, this, &CharacterInspector::CommitBone);
  }
  connect(&smart_, &QCheckBox::toggled, this, [this](bool is_on) {
    if (is_filling_) return;
    Report(characters_->SetSmart(session_->character(), session_->bone(), is_on));
  });
  return body;
}

QWidget* CharacterInspector::BuildAction() {
  auto* body = new QWidget(this);
  auto* layout = new QVBoxLayout(body);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(theme::kRowGap / 2);
  action_turn_.setOrientation(Qt::Horizontal);
  action_turn_.setRange(-180, 180);
  action_turn_.setTickPosition(QSlider::TicksBelow);
  action_turn_.setTickInterval(45);
  action_turn_.setToolTip(
      tr("Turn the smart bone to see its action; double click goes back to rest"));
  layout->addWidget(&action_turn_text_);
  layout->addWidget(&action_turn_);
  stops_.setMaximumHeight(110);
  stops_.setToolTip(tr("The turns where the action is posed"));
  layout->addWidget(&stops_);
  add_stop_.setText(tr("Add stop"));
  add_stop_.setToolTip(tr("Save a stop at the turn shown, then pose it"));
  pose_stop_.setText(tr("Pose stop"));
  pose_stop_.setCheckable(true);
  pose_stop_.setToolTip(tr("While on, what you move on the canvas is saved "
                           "into the picked stop"));
  remove_stop_.setText(tr("Remove"));
  layout->addWidget(TileRow({&add_stop_, &pose_stop_, &remove_stop_}, body));
  connect(&action_turn_, &QSlider::valueChanged, this, [this](int value) {
    if (is_filling_) return;
    session_->SetAction(session_->bone(), -1, value);
  });
  connect(&add_stop_, &QPushButton::clicked, this, [this] {
    const auto stop = characters_->AddActionStop(
        session_->character(), session_->bone(), action_turn_.value());
    if (!stop) {
      emit Problem(stop.error().message);
      return;
    }
    session_->SetAction(session_->bone(), *stop, action_turn_.value());
    emit Problem(QString());
  });
  connect(&stops_, &QListWidget::currentRowChanged, this, [this](int row) {
    if (is_filling_ || row < 0) return;
    const Character* character = Shown();
    const SmartAction* action =
        character != nullptr ? FindAction(*character, session_->bone()) : nullptr;
    if (action == nullptr || static_cast<size_t>(row) >= action->stops.size()) {
      return;
    }
    const double turn = action->stops[static_cast<size_t>(row)].turn;
    session_->SetAction(session_->bone(), pose_stop_.isChecked() ? row : -1, turn);
  });
  connect(&pose_stop_, &QPushButton::toggled, this, [this](bool is_on) {
    if (is_filling_) return;
    const int row = stops_.currentRow();
    if (is_on && row < 0) {
      emit Problem(tr("Add or pick a stop to pose."));
      pose_stop_.setChecked(false);
      return;
    }
    session_->SetAction(session_->bone(), is_on ? row : -1,
                        session_->action_turn());
  });
  connect(&remove_stop_, &QPushButton::clicked, this, [this] {
    const int row = stops_.currentRow();
    Report(characters_->RemoveActionStop(session_->character(), session_->bone(),
                                         row));
    session_->SetAction(session_->bone(), -1, 0.0);
  });
  return body;
}

void CharacterInspector::CommitPart() {
  if (is_filling_) {
    return;
  }
  const Character* character = Shown();
  assert(session_ != nullptr);
  const CharacterPart* part =
      character != nullptr ? FindPart(*character, session_->part()) : nullptr;
  if (part == nullptr) {
    return;
  }
  CharacterPart rest = *part;
  rest.offset = QPointF(x_.value(), y_.value());
  rest.rotation = turn_.value();
  rest.scale_x = scale_x_.value();
  rest.scale_y = scale_y_.value();
  rest.opacity = opacity_.value();
  rest.is_flipped = flipped_.isChecked();
  Report(characters_->SetPartRest(session_->character(), part->id, rest));
}

void CharacterInspector::CommitLook() {
  if (is_filling_) {
    return;
  }
  assert(session_ != nullptr);
  VectorShape style = session_->style();
  style.has_fill = has_fill_.isChecked();
  style.fill = fill_.color();
  style.has_stroke = has_stroke_.isChecked();
  style.stroke = stroke_.color();
  style.stroke_width = stroke_width_.value();
  style.brush = static_cast<LineBrush>(std::clamp(brush_.currentIndex(), 0,
                                                  kLineBrushCount - 1));
  session_->SetStyle(style);
  if (session_->shape() >= 0) {
    Report(characters_->SetShapeStyle(session_->character(), session_->part(),
                                      session_->shape(), style));
  }
}

void CharacterInspector::CommitBone() {
  if (is_filling_) {
    return;
  }
  const Character* character = Shown();
  assert(session_ != nullptr);
  const Bone* bone =
      character != nullptr ? FindBone(*character, session_->bone()) : nullptr;
  if (bone == nullptr) {
    return;
  }
  Bone changed = *bone;
  changed.strength = strength_.value();
  changed.has_limits = has_limits_.isChecked();
  changed.min_turn = min_turn_.value();
  changed.max_turn = max_turn_.value();
  changed.is_locked = locked_.isChecked();
  changed.dynamics = dynamics_.value();
  Report(characters_->SetBone(session_->character(), changed));
}

void CharacterInspector::RefreshPart(const Character& character,
                                     const CharacterPart& part) {
  assert(part.id.IsValid());
  assert(IsSound(character));
  static const char* const kKinds[] = {"Group", "Bones", "Drawing", "Switch",
                                       "Picture"};
  part_kind_.setText(tr(kKinds[static_cast<int>(part.kind)]));
  x_.setValue(part.offset.x());
  y_.setValue(part.offset.y());
  turn_.setValue(part.rotation);
  scale_x_.setValue(part.scale_x);
  scale_y_.setValue(part.scale_y);
  opacity_.setValue(part.opacity);
  flipped_.setChecked(part.is_flipped);
  const CharacterPart* bones = BoneParentOf(character, part.id);
  const bool is_bendable = bones != nullptr && part.kind != PartKind::kBone;
  bind_.setEnabled(is_bendable);
  bind_.setToolTip(is_bendable ? tr("How bones move this part")
                               : tr("Put this part inside a bone part to bind "
                                    "it to bones"));
  bind_.setCurrentIndex(bind_.findData(static_cast<int>(part.bind)));
  bind_bone_.clear();
  bind_bone_.addItem(tr("None"), 0);
  if (bones != nullptr) {
    for (const Bone& bone : bones->bones) {
      bind_bone_.addItem(bone.name, bone.id.value());
    }
  }
  bind_bone_.setCurrentIndex(std::max(0, bind_bone_.findData(part.bound_bone.value())));
  bind_bone_.setEnabled(is_bendable && part.bind == BindMode::kLayer);
  // Only what the part can use shows: binding inside a bone part, the
  // rest pick on a switch.
  part_form_->setRowVisible(&bind_, is_bendable);
  part_form_->setRowVisible(&bind_bone_,
                            is_bendable && part.bind == BindMode::kLayer);
  const bool is_switch = part.kind == PartKind::kSwitch;
  part_form_->setRowVisible(&rest_pick_, is_switch);
  rest_pick_.clear();
  for (PartId child : ChildrenOf(character, part.id)) {
    rest_pick_.addItem(FindPart(character, child)->name, child.value());
  }
  rest_pick_.setCurrentIndex(std::max(0, rest_pick_.findData(part.rest_pick.value())));
}

void CharacterInspector::RefreshLook(const CharacterPart* part) {
  assert(session_ != nullptr);
  assert(session_->shape() >= -1);
  const bool has_shape = part != nullptr && session_->shape() >= 0 &&
                         static_cast<size_t>(session_->shape()) <
                             part->drawing.shapes.size();
  const VectorShape& style =
      has_shape ? part->drawing.shapes[static_cast<size_t>(session_->shape())]
                : session_->style();
  look_for_.setText(has_shape ? tr("The picked shape:")
                              : tr("New shapes are drawn with:"));
  has_fill_.setChecked(style.has_fill);
  fill_.SetColor(style.fill);
  has_stroke_.setChecked(style.has_stroke);
  stroke_.SetColor(style.stroke);
  stroke_width_.setValue(style.stroke_width);
  brush_.setCurrentIndex(static_cast<int>(style.brush));
  raise_.setEnabled(has_shape);
  lower_.setEnabled(has_shape);
  const QString why_shape = tr("Pick a shape first");
  raise_.setToolTip(has_shape ? tr("Bring the shape forward") : why_shape);
  lower_.setToolTip(has_shape ? tr("Send the shape back") : why_shape);
  const bool has_points = part != nullptr && !session_->points().empty();
  make_shape_.setEnabled(has_points);
  make_shape_.setToolTip(has_points
                             ? tr("Fill the picked points' closed lines")
                             : tr("Pick the points of a closed line first"));
  curvature_.setEnabled(has_points);
  point_width_.setEnabled(has_points);
  weld_.setEnabled(session_->points().size() == 2);
  weld_.setToolTip(session_->points().size() == 2
                       ? tr("Join the two picked points into one")
                       : tr("Pick exactly two points to weld"));
  if (has_points) {
    const VectorPoint* first = FindPoint(part->drawing, *session_->points().begin());
    if (first != nullptr) {
      curvature_.setValue(first->curvature);
      point_width_.setValue(first->width);
    }
  }
}

void CharacterInspector::RefreshBone(const Character& character,
                                     const Bone& bone) {
  assert(bone.id.IsValid());
  assert(IsSound(character));
  bone_name_.setText(bone.name);
  strength_.setValue(bone.strength);
  has_limits_.setChecked(bone.has_limits);
  min_turn_.setValue(bone.min_turn);
  max_turn_.setValue(bone.max_turn);
  min_turn_.setEnabled(bone.has_limits);
  max_turn_.setEnabled(bone.has_limits);
  locked_.setChecked(bone.is_locked);
  dynamics_.setValue(bone.dynamics);
  smart_.setChecked(bone.is_smart);
  const SmartAction* action = FindAction(character, bone.id);
  action_section_->setVisible(bone.is_smart && action != nullptr);
  if (action == nullptr) {
    return;
  }
  const bool is_this = session_->action_bone() == bone.id;
  const double turn = is_this ? session_->action_turn() : 0.0;
  action_turn_.setValue(static_cast<int>(std::lround(turn)));
  action_turn_text_.setText(tr("Turn: %1 deg").arg(std::lround(turn)));
  stops_.clear();
  for (const ActionStop& stop : action->stops) {
    stops_.addItem(tr("Stop at %1 deg").arg(std::lround(stop.turn)));
  }
  const int posing = is_this ? session_->action_stop() : -1;
  if (posing >= 0) {
    stops_.setCurrentRow(posing);
  }
  pose_stop_.setChecked(posing >= 0);
  remove_stop_.setEnabled(stops_.count() > 0);
}

void CharacterInspector::Refresh() {
  assert(history_ != nullptr);
  assert(session_ != nullptr);
  is_filling_ = true;
  const Character* character = Shown();
  const CharacterPart* part =
      character != nullptr ? FindPart(*character, session_->part()) : nullptr;
  part_section_->setVisible(part != nullptr);
  if (part != nullptr) {
    RefreshPart(*character, *part);
  }
  const bool is_drawing = part != nullptr && part->kind == PartKind::kVector;
  look_section_->setVisible(is_drawing);
  RefreshLook(is_drawing ? part : nullptr);
  const Bone* bone =
      character != nullptr ? FindBone(*character, session_->bone()) : nullptr;
  bone_section_->setVisible(bone != nullptr);
  action_section_->setVisible(false);
  if (bone != nullptr) {
    RefreshBone(*character, *bone);
  }
  is_filling_ = false;
}

}  // namespace snapper
