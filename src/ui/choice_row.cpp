#include "ui/choice_row.h"

#include <cassert>
#include <memory>

namespace snapper {

ChoiceRow::ChoiceRow(QWidget* parent) : QWidget(parent), layout_(this) {
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(2);
  assert(parent == nullptr || parent->isWidgetType());
  group_.setExclusive(true);
  connect(&group_, &QButtonGroup::idClicked, this,
          [this](int index) { emit currentIndexChanged(index); });
  assert(group_.exclusive());
}

void ChoiceRow::addItem(const QString& text, const QVariant& value) {
  assert(!text.isEmpty());
  assert(buttons_.size() < 16);
  // A Qt child of the row, which owns it.
  QToolButton* button = std::make_unique<QToolButton>(this).release();
  button->setText(text);
  button->setCheckable(true);
  button->setToolButtonStyle(Qt::ToolButtonTextOnly);
  const int index = count();
  group_.addButton(button, index);
  layout_.addWidget(button);
  buttons_.push_back(button);
  data_.push_back(value);
  const bool is_first = index == 0;
  if (is_first) {
    button->setChecked(true);
  }
}

void ChoiceRow::setCurrentIndex(int index) {
  assert(index >= -1);
  assert(index < count());
  const bool was_index = index != currentIndex();
  const bool is_choice = index >= 0;
  if (is_choice) {
    buttons_[static_cast<size_t>(index)]->setChecked(true);
  }
  // A combo box reports a change made in code too.
  const bool is_change = was_index && is_choice;
  if (is_change) {
    emit currentIndexChanged(index);
  }
}

void ChoiceRow::setItemText(int index, const QString& text) {
  assert(index >= 0 && index < count());
  assert(!text.isEmpty());
  buttons_[static_cast<size_t>(index)]->setText(text);
}

QVariant ChoiceRow::currentData() const {
  const int index = currentIndex();
  assert(data_.size() == buttons_.size());
  assert(index < count());
  const bool is_choice = index >= 0;
  return is_choice ? data_[static_cast<size_t>(index)] : QVariant();
}

}  // namespace snapper
