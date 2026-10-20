#include "ui/character_page.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QVBoxLayout>

#include <cassert>

#include "edit/history_manager.h"
#include "ui/panel_kit.h"
#include "ui/theme.h"
#include "ui/tool_button.h"
#include "ui/tool_icons.h"

namespace snapper {
namespace {

// One tool button: its tool, label, key, what it does, and which parts
// it works on.
struct ToolSpec final {
  CharacterTool tool;
  const char* label;
  Qt::Key key;
  const char* help;
  bool on_drawing;
  bool on_bones;
  bool on_other;
};

constexpr ToolSpec kTools[kCharacterToolCount] = {
    {CharacterTool::kPick, "Pick", Qt::Key_V,
     "Pick and drag points, shapes or bones; drag on empty space to box pick",
     true, true, true},
    {CharacterTool::kMovePart, "Move part", Qt::Key_T,
     "Drag to move the picked part where it rests", true, true, true},
    {CharacterTool::kAddPoint, "Add point", Qt::Key_A,
     "Drag to draw a line; click to go on from the picked end. On a bone "
     "part, drag to add a bone",
     true, true, false},
    {CharacterTool::kFreehand, "Freehand", Qt::Key_F,
     "Draw a smooth line; end where you started to fill it", true, false,
     false},
    {CharacterTool::kOval, "Oval", Qt::Key_O,
     "Drag an oval; Shift for a circle", true, false, false},
    {CharacterTool::kRectangle, "Rectangle", Qt::Key_R,
     "Drag a rectangle; Shift for a square", true, false, false},
    {CharacterTool::kCurvature, "Curvature", Qt::Key_C,
     "Drag left to sharpen, right to round the picked points", true, false,
     false},
    {CharacterTool::kLineWidth, "Line width", Qt::Key_W,
     "Drag up to thicken, down to thin the line at the picked points (or "
     "the point you press)",
     true, false, false},
    {CharacterTool::kDeleteEdge, "Delete edge", Qt::Key_D,
     "Click a line between two points to cut it out; a fill it outlined "
     "goes with it",
     true, false, false},
    {CharacterTool::kMagnet, "Magnet", Qt::Key_M,
     "Drag to pull the points near where you press, the nearest most; "
     "[ and ] change its reach",
     true, false, false},
    {CharacterTool::kFill, "Paint", Qt::Key_P,
     "Click a shape to give it the current fill and line", true, false,
     false},
    {CharacterTool::kStrength, "Bone reach", Qt::Key_S,
     "Drag on a bone to set how far it pulls points", true, true, true},
    {CharacterTool::kBindPoints, "Bind points", Qt::Key_I,
     "Click a bone, then click or box the points it carries; Ctrl unbinds",
     true, false, false},
    {CharacterTool::kTryBones, "Try bones", Qt::Key_Z,
     "Drag bones to try the rig; they spring back. While posing a smart "
     "bone stop, what you drag is kept",
     true, true, true},
};

}  // namespace

CharacterPage::CharacterPage(HistoryManager* history,
                             CharacterManager* characters,
                             const QString& settings_path, QWidget* parent)
    : QWidget(parent),
      history_(history),
      split_(Qt::Horizontal),
      canvas_(history, characters, &session_),
      parts_(history, characters, &session_),
      inspector_(history, characters, &session_) {
  assert(history_ != nullptr && characters != nullptr);
  assert(settings_path.size() < 100000);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  inspector_scroll_.setWidgetResizable(true);
  inspector_scroll_.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  inspector_scroll_.setFrameShape(QFrame::NoFrame);
  inspector_scroll_.setWidget(&inspector_);
  inspector_scroll_.setMinimumWidth(kSideMinWidth);
  parts_.setMinimumWidth(kSideMinWidth);
  split_.addWidget(&parts_);
  auto* stage = new QWidget(&split_);
  auto* stage_row = new QHBoxLayout(stage);
  stage_row->setContentsMargins(0, 0, 0, 0);
  stage_row->setSpacing(0);
  stage_row->addWidget(BuildTools());
  stage_row->addWidget(&canvas_, 1);
  split_.addWidget(stage);
  split_.addWidget(&inspector_scroll_);
  split_.SetDefaultShares({kSideShare, kMainShare, kSideShare});
  split_.Remember(QStringLiteral("character"), settings_path);
  layout->addWidget(&split_, 1);
  hint_.setContentsMargins(theme::kIndent, 2, theme::kIndent, 2);
  theme::SetLevel(&hint_, theme::kWell);
  layout->addWidget(&hint_);
  auto show_problem = [this](const QString& message) {
    hint_.setText(message);
    emit Problem(message);
  };
  connect(&canvas_, &CharacterCanvas::Problem, this, show_problem);
  connect(&parts_, &CharacterPartsPanel::Problem, this, show_problem);
  connect(&inspector_, &CharacterInspector::Problem, this, show_problem);
  connect(&session_, &CharacterSession::Changed, this,
          &CharacterPage::RefreshTools);
  connect(history_, &HistoryManager::Changed, this, &CharacterPage::RefreshTools);
  RefreshTools();
}

QWidget* CharacterPage::BuildTools() {
  // A tool box as Krita has, down the left of the canvas: two columns of
  // icons, a gap between moving, drawing, shaping and bone tools.
  auto* box = new QWidget(this);
  theme::SetLevel(box, theme::kRaised);
  auto* grid = new QGridLayout(box);
  grid->setContentsMargins(4, 4, 4, 4);
  grid->setSpacing(2);
  tool_group_.setExclusive(true);
  int row = 0;
  int column = 0;
  for (int i = 0; i < kCharacterToolCount; ++i) {
    const ToolSpec& spec = kTools[i];
    auto* button = new ToolButton(ToolIcon(spec.tool), tr(spec.label),
                                  QKeySequence(spec.key).toString(), box);
    tool_group_.addButton(button, i);
    tool_buttons_[static_cast<size_t>(i)] = button;
    grid->addWidget(button, row, column);
    column = 1 - column;
    row += column == 0 ? 1 : 0;
    const bool ends_group = spec.tool == CharacterTool::kMovePart ||
                            spec.tool == CharacterTool::kRectangle ||
                            spec.tool == CharacterTool::kFill;
    if (ends_group) {
      row += column == 0 ? 0 : 1;
      column = 0;
      grid->setRowMinimumHeight(row, theme::kCellGap / 2);
      ++row;
    }
  }
  grid->setRowStretch(row + 1, 1);
  tool_buttons_[0]->setChecked(true);
  connect(&tool_group_, &QButtonGroup::idClicked, this, [this](int id) {
    session_.SetTool(static_cast<CharacterTool>(id));
    canvas_.setFocus();
  });
  return box;
}

void CharacterPage::RefreshTools() {
  assert(history_ != nullptr);
  assert(tool_buttons_[0] != nullptr);
  const Character* character = canvas_.Shown();
  const CharacterPart* part =
      character != nullptr ? FindPart(*character, session_.part()) : nullptr;
  for (int i = 0; i < kCharacterToolCount; ++i) {
    const ToolSpec& spec = kTools[i];
    QAbstractButton* button = tool_buttons_[static_cast<size_t>(i)];
    const bool is_drawing = part != nullptr && part->kind == PartKind::kVector;
    const bool is_bones = part != nullptr && part->kind == PartKind::kBone;
    const bool is_usable = part == nullptr ? spec.tool == CharacterTool::kPick
                           : is_drawing    ? spec.on_drawing
                           : is_bones      ? spec.on_bones
                                           : spec.on_other;
    button->setEnabled(is_usable);
    const QString why =
        part == nullptr ? tr("Pick a part on the left first")
        : spec.on_drawing && !spec.on_bones
            ? tr("Works on a drawing part")
            : tr("Works on a drawing or bone part");
    button->setToolTip(QStringLiteral("%1 (%2)\n%3")
                           .arg(tr(spec.label),
                                QKeySequence(spec.key).toString(),
                                is_usable ? tr(spec.help) : why));
    button->setChecked(session_.tool() == spec.tool);
  }
  const bool is_tool_usable =
      tool_buttons_[static_cast<size_t>(session_.tool())]->isEnabled();
  if (!is_tool_usable) {
    session_.SetTool(CharacterTool::kPick);
  }
}

void CharacterPage::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(tool_buttons_[0] != nullptr);
  const bool is_plain = event->modifiers() == Qt::NoModifier ||
                        event->modifiers() == Qt::KeypadModifier;
  if (is_plain) {
    for (int i = 0; i < kCharacterToolCount; ++i) {
      if (kTools[i].key == event->key() &&
          tool_buttons_[static_cast<size_t>(i)]->isEnabled()) {
        session_.SetTool(kTools[i].tool);
        return;
      }
    }
  }
  QWidget::keyPressEvent(event);
}

}  // namespace snapper
