#include "ui/draw_bar.h"

#include <QPainter>

#include <cassert>
#include <memory>
#include <utility>

#include "ui/deck_paint.h"
#include "ui/hover_fade.h"
#include "ui/problem.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

struct ToolFace final {
  const char* name;
  const char* key;
  const char* tip;
  TapeGlyph glyph;
};

// In DrawTool order.
constexpr std::array<ToolFace, kDrawToolCount> kToolFaces = {{
    {"Brush", "B", "A hard pixel brush. Right button erases; Shift+click "
                   "draws a line from the last point; [ and ] size it.",
     TapeGlyph::kBrush},
    {"Eraser", "E", "The same brush, clearing to transparent; E again "
                    "goes back to the brush.",
     TapeGlyph::kEraser},
    {"Fill", "F", "Fills the touching area of the same color.",
     TapeGlyph::kFill},
    {"Pick", "I", "Click to take a color (hold Alt with any tool).",
     TapeGlyph::kPick},
}};

void Paint(QWidget* widget) {
  assert(widget != nullptr);
  QPalette look = widget->palette();
  look.setColor(QPalette::Window, theme::kFace);
  widget->setPalette(look);
  widget->setAutoFillBackground(true);
}

}  // namespace

DrawBar::DrawBar(const Managers& managers)
    : managers_(managers), layout_(this),
      onion_(tr("Onion")), discard_(tr("Discard")), keep_(tr("Keep drawing")),
      done_(tr("Done")), cancel_(tr("Cancel")) {
  assert(managers_.draw != nullptr);
  assert(managers_.toasts != nullptr);
  Paint(this);
  QFont bold = title_.font();
  bold.setBold(true);
  title_.setFont(bold);
  name_.setPlaceholderText(tr("Name"));
  name_.setToolTip(tr("What the drawing is called in the Drawing picker."));
  name_.setMaximumWidth(140);
  done_.setToolTip(tr("Save the drawing (Enter)."));
  cancel_.setToolTip(tr("Leave without saving (Escape)."));
  question_.setText(tr("Throw away what you drew?"));
  layout_.addWidget(&title_);
  layout_.addWidget(&name_);
  layout_.addWidget(&onion_);
  layout_.addWidget(&size_);
  layout_.addStretch(1);
  layout_.addWidget(&question_);
  layout_.addWidget(&discard_);
  layout_.addWidget(&keep_);
  layout_.addWidget(&done_);
  layout_.addWidget(&cancel_);
  for (QPushButton* button : {&discard_, &keep_, &done_, &cancel_}) {
    button->setFocusPolicy(Qt::NoFocus);
  }
  onion_.setFocusPolicy(Qt::NoFocus);
  DrawManager* draw = managers_.draw;
  connect(draw, &DrawManager::Changed, this, &DrawBar::Refresh);
  connect(draw, &DrawManager::Started, this, [this] {
    name_.setText(managers_.draw->DefaultName());
    Ask(false);
  });
  connect(&onion_, &QCheckBox::toggled, draw, &DrawManager::SetOnion);
  // Enter in the name field reaches the canvas, which presses Done.
  connect(&done_, &QPushButton::clicked, this, &DrawBar::Finish);
  connect(&cancel_, &QPushButton::clicked, this, &DrawBar::AskCancel);
  connect(&discard_, &QPushButton::clicked, draw, &DrawManager::Cancel);
  connect(&keep_, &QPushButton::clicked, this, [this] { Ask(false); });
  Ask(false);
}

void DrawBar::Finish() {
  assert(managers_.draw != nullptr);
  assert(managers_.toasts != nullptr);
  const QString problem = ProblemOf(managers_.draw->Done(name_.text()));
  if (!problem.isEmpty()) {
    managers_.toasts->Problem(problem);
  }
}

void DrawBar::AskCancel() {
  assert(managers_.draw != nullptr);
  const bool is_asking = !question_.isHidden();
  if (managers_.draw->is_dirty() && !is_asking) {
    Ask(true);
    return;
  }
  managers_.draw->Cancel();
}

void DrawBar::Refresh() {
  const DrawManager* draw = managers_.draw;
  assert(draw != nullptr);
  title_.setText(draw->Title());
  size_.setText(tr("Brush %1 px").arg(draw->brush_size()));
  // A storyboard frame has no name; its onion is the frame before.
  name_.setVisible(draw->is_named());
  onion_.setToolTip(draw->is_named()
                        ? tr("Show the original under yours at 30 % (D). "
                             "Hold R to see only the original.")
                        : tr("Show the frame before (red) faint under "
                             "yours, to trace from (D). Hold R to see "
                             "this frame as it was."));
  const QSignalBlocker quiet(onion_);
  onion_.setChecked(draw->is_onion());
  // No title while no drawing is open, so the text may be null.
  assert(onion_.isChecked() == draw->is_onion());
}

void DrawBar::Ask(bool is_asking) {
  assert(managers_.draw != nullptr);
  question_.setVisible(is_asking);
  discard_.setVisible(is_asking);
  keep_.setVisible(is_asking);
  done_.setVisible(!is_asking);
  cancel_.setVisible(!is_asking);
}

DrawTools::DrawTools(const Managers& managers)
    : managers_(managers), layout_(this), well_(managers) {
  assert(managers_.draw != nullptr);
  layout_.setContentsMargins(6, 8, 6, 8);
  layout_.setSpacing(4);
  for (int i = 0; i < kDrawToolCount; ++i) {
    const ToolFace& face = kToolFaces[static_cast<size_t>(i)];
    auto tile = std::make_unique<TapeKey>(face.glyph);
    tile->setText(tr(face.name));
    tile->SetCornerMark(QString::fromLatin1(face.key));
    tile->setToolTip(
        QStringLiteral("%1: %2 (%3)").arg(tr(face.name), tr(face.tip),
                                          face.key));
    tile->setCheckable(true);
    tile->setFocusPolicy(Qt::NoFocus);
    tile->setFixedSize(52, 48);
    // The key owns it; it sinks the key as it is pressed.
    std::make_unique<HoverFade>(tile.get(), managers_.motion).release();
    layout_.addWidget(tile.get());
    connect(tile.get(), &QPushButton::clicked, this, [this, i] {
      managers_.draw->SetTool(static_cast<DrawTool>(i));
    });
    tiles_[static_cast<size_t>(i)] = std::move(tile);
  }
  layout_.addSpacing(8);
  layout_.addWidget(&well_);
  layout_.addStretch(1);
  connect(managers_.draw, &DrawManager::Changed, this, &DrawTools::Refresh);
  Refresh();
}

void DrawTools::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(width() >= 0);
  // A strip of deck panel the keys stand in.
  QPainter painter(this);
  deck::PaintPanel(&painter, QRectF(rect()));
}

void DrawTools::Refresh() {
  assert(managers_.draw != nullptr);
  const int tool = static_cast<int>(managers_.draw->tool());
  assert(tool >= 0 && tool < kDrawToolCount);
  for (int i = 0; i < kDrawToolCount; ++i) {
    tiles_[static_cast<size_t>(i)]->setChecked(i == tool);
  }
}

}  // namespace snapper
