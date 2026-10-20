#include "ui/character_styles_panel.h"

#include <QGridLayout>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <cassert>

#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "ui/form_helpers.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kStyleIdRole = Qt::UserRole;
constexpr int kSwatch = 18;

// A square of the style's fill crossed by its line.
QPixmap SwatchOf(const VectorShape& look) {
  assert(look.stroke_width >= 0.0);
  assert(kSwatch > 4);
  QPixmap swatch(kSwatch, kSwatch);
  swatch.fill(Qt::transparent);
  QPainter painter(&swatch);
  painter.setRenderHint(QPainter::Antialiasing);
  if (look.has_fill) {
    painter.fillRect(QRect(1, 1, kSwatch - 2, kSwatch - 2), look.fill);
  }
  if (look.has_stroke) {
    painter.setPen(QPen(look.stroke, 3.0, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(3, kSwatch - 4), QPointF(kSwatch - 4, 3));
  }
  painter.setPen(theme::kShadow);
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(QRect(0, 0, kSwatch - 1, kSwatch - 1));
  return swatch;
}

}  // namespace

CharacterStylesPanel::CharacterStylesPanel(HistoryManager* history,
                                           CharacterManager* characters,
                                           CharacterSession* session,
                                           QWidget* parent)
    : QWidget(parent),
      history_(history),
      characters_(characters),
      session_(session),
      new_(tr("New")),
      use_(tr("Use")),
      own_(tr("Own look")),
      delete_(tr("Delete")) {
  assert(history_ != nullptr && characters_ != nullptr);
  assert(session_ != nullptr);
  auto* column = new QVBoxLayout(this);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap);
  list_.setObjectName(QStringLiteral("styles"));
  list_.setMaximumHeight(140);
  list_.setIconSize(QSize(kSwatch, kSwatch));
  list_.setToolTip(tr("Named looks shapes share: change a style and every "
                      "shape using it changes. Double click to rename."));
  column->addWidget(&list_);
  auto* keys = new QGridLayout();
  keys->setSpacing(theme::kCellGap);
  keys->addWidget(&new_, 0, 0);
  keys->addWidget(&use_, 0, 1);
  keys->addWidget(&own_, 1, 0);
  keys->addWidget(&delete_, 1, 1);
  column->addLayout(keys);
  connect(&new_, &QPushButton::clicked, this, &CharacterStylesPanel::New);
  connect(&use_, &QPushButton::clicked, this, &CharacterStylesPanel::Use);
  connect(&own_, &QPushButton::clicked, this, &CharacterStylesPanel::OwnLook);
  connect(&delete_, &QPushButton::clicked, this,
          &CharacterStylesPanel::Delete);
  connect(&list_, &QListWidget::itemChanged, this,
          &CharacterStylesPanel::Rename);
  connect(&list_, &QListWidget::itemSelectionChanged, this,
          &CharacterStylesPanel::Refresh);
  connect(history_, &HistoryManager::Changed, this,
          &CharacterStylesPanel::Refresh);
  connect(session_, &CharacterSession::Changed, this,
          &CharacterStylesPanel::Refresh);
  Refresh();
}

const Character* CharacterStylesPanel::Shown() const {
  assert(history_ != nullptr);
  assert(session_ != nullptr);
  return session_->character().isEmpty()
             ? nullptr
             : FindCharacter(history_->current(), session_->character());
}

bool CharacterStylesPanel::HasShape() const {
  const Character* character = Shown();
  const CharacterPart* part =
      character != nullptr ? FindPart(*character, session_->part()) : nullptr;
  return part != nullptr && session_->shape() >= 0 &&
         static_cast<size_t>(session_->shape()) < part->drawing.shapes.size();
}

StyleId CharacterStylesPanel::Picked() const {
  assert(list_.count() >= 0);
  const QListWidgetItem* item = list_.currentItem();
  const bool is_picked = item != nullptr && item->isSelected();
  return is_picked ? StyleId(item->data(kStyleIdRole).toInt()) : StyleId();
}

void CharacterStylesPanel::Refresh() {
  if (is_filling_) {
    return;
  }
  is_filling_ = true;
  const Character* character = Shown();
  const StyleId was = Picked();
  {
    const QSignalBlocker quiet(list_);
    list_.clear();
    if (character != nullptr) {
      for (const VectorStyle& style : character->styles) {
        auto* item = new QListWidgetItem(QIcon(SwatchOf(style.look)),
                                         style.name, &list_);
        item->setData(kStyleIdRole, style.id.value());
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        if (style.id == was) {
          list_.setCurrentItem(item);
          item->setSelected(true);
        }
      }
    }
  }
  const bool has_style = Picked().IsValid();
  const bool has_shape = HasShape();
  const QString no_style = tr("Pick a style in the list first.");
  Explain(&new_, character != nullptr ? QString() : tr("Open a character."),
          has_shape ? tr("Make a style from the picked shape's look and "
                         "give it that style")
                    : tr("Make a style from the look new shapes take"));
  Explain(&use_, has_style ? QString() : no_style,
          has_shape ? tr("Give the picked shape this style")
                    : tr("Draw new shapes with this style"));
  const VectorShape& look_new = session_->style();
  const bool has_link =
      has_shape ? false : look_new.style.IsValid();
  bool shape_link = false;
  if (has_shape && character != nullptr) {
    const CharacterPart* part = FindPart(*character, session_->part());
    shape_link = part->drawing.shapes[static_cast<size_t>(session_->shape())]
                     .style.IsValid();
  }
  Explain(&own_,
          shape_link || has_link ? QString()
                                 : tr("It already has its own look."),
          tr("Keep the look but no longer follow the style"));
  Explain(&delete_, has_style ? QString() : no_style,
          tr("Delete the style; its shapes keep their look"));
  is_filling_ = false;
}

void CharacterStylesPanel::New() {
  assert(characters_ != nullptr);
  assert(session_ != nullptr);
  const Character* character = Shown();
  if (character == nullptr) {
    return;
  }
  const bool has_shape = HasShape();
  const CharacterPart* part = FindPart(*character, session_->part());
  const VectorShape look =
      has_shape
          ? LookOf(*character,
                   part->drawing.shapes[static_cast<size_t>(session_->shape())])
          : LookOf(*character, session_->style());
  const Result<StyleId> made =
      characters_->AddStyle(session_->character(), QString(), look);
  if (!made) {
    Report(made.error().message);
    return;
  }
  if (has_shape) {
    const auto used = characters_->UseStyle(
        session_->character(), session_->part(), session_->shape(), *made);
    Report(used ? QString() : used.error().message);
  } else {
    VectorShape style = look;
    style.style = *made;
    session_->SetStyle(style);
  }
}

void CharacterStylesPanel::Use() {
  assert(characters_ != nullptr);
  const StyleId id = Picked();
  const Character* character = Shown();
  if (!id.IsValid() || character == nullptr) {
    return;
  }
  if (HasShape()) {
    const auto done = characters_->UseStyle(
        session_->character(), session_->part(), session_->shape(), id);
    Report(done ? QString() : done.error().message);
    return;
  }
  const VectorStyle* style = FindStyle(*character, id);
  if (style != nullptr) {
    VectorShape look = style->look;
    look.style = id;
    session_->SetStyle(look);
  }
}

void CharacterStylesPanel::OwnLook() {
  assert(characters_ != nullptr);
  const Character* character = Shown();
  if (character == nullptr) {
    return;
  }
  if (HasShape()) {
    const auto done = characters_->UseStyle(
        session_->character(), session_->part(), session_->shape(), StyleId());
    Report(done ? QString() : done.error().message);
    return;
  }
  VectorShape look = LookOf(*character, session_->style());
  look.style = StyleId();
  session_->SetStyle(look);
}

void CharacterStylesPanel::Delete() {
  assert(characters_ != nullptr);
  const StyleId id = Picked();
  if (!id.IsValid()) {
    return;
  }
  if (session_->style().style == id) {
    VectorShape look = session_->style();
    look.style = StyleId();
    session_->SetStyle(look);
  }
  const auto done = characters_->RemoveStyle(session_->character(), id);
  Report(done ? QString() : done.error().message);
}

void CharacterStylesPanel::Rename(QListWidgetItem* item) {
  assert(item != nullptr);
  assert(characters_ != nullptr);
  if (is_filling_) {
    return;
  }
  const auto done = characters_->RenameStyle(
      session_->character(), StyleId(item->data(kStyleIdRole).toInt()),
      item->text());
  Report(done ? QString() : done.error().message);
  if (!done) {
    Refresh();
  }
}

}  // namespace snapper
