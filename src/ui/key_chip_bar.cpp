#include "ui/key_chip_bar.h"

#include <QSizePolicy>

#include <algorithm>
#include <cassert>
#include <memory>

#include "base/text.h"
#include "edit/key_manager.h"
#include "edit/selection_manager.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"
#include "ui/space_keys_action.h"

namespace snapper {
namespace {

// Pixels between the bar and the keys it floats over.
constexpr int kGap = 3;

QString EaseText(Ease ease) {
  assert(static_cast<int>(ease) >= 0);
  assert(static_cast<int>(ease) < kEaseCount);
  switch (ease) {
    case Ease::kStep:
      return QObject::tr("Step");
    case Ease::kLinear:
      return QObject::tr("Line");
    case Ease::kEaseIn:
    case Ease::kEaseOut:
    case Ease::kEaseInOut:
      return QObject::tr("Ease");
  }
  return QString();
}

QString EaseHelp(Ease ease) {
  assert(static_cast<int>(ease) >= 0);
  assert(static_cast<int>(ease) < kEaseCount);
  switch (ease) {
    case Ease::kStep:
      return QObject::tr("Hold the pose until the next key, then jump.");
    case Ease::kLinear:
      return QObject::tr("Glide to the next key at an even speed.");
    case Ease::kEaseIn:
    case Ease::kEaseOut:
    case Ease::kEaseInOut:
      return QObject::tr("Glide to the next key, slow at both ends.");
  }
  return QString();
}

}  // namespace

KeyChipBar::KeyChipBar(const Managers& managers, QWidget* parent)
    : QWidget(parent), managers_(managers), layout_(this) {
  assert(managers_.IsComplete());
  layout_.setContentsMargins(3, 3, 3, 3);
  layout_.setSpacing(2);
  setAutoFillBackground(true);
  setFocusPolicy(Qt::NoFocus);
  for (size_t i = 0; i < kChipEases.size(); ++i) {
    const Ease ease = kChipEases[i];
    eases_[i] = AddChip(EaseText(ease), QStringLiteral("ease_%1").arg(i));
    eases_[i]->setCheckable(true);
    eases_[i]->setToolTip(EaseHelp(ease));
    connect(eases_[i], &QPushButton::clicked, this, [this, ease] {
      emit Problem(ProblemOf(managers_.keys->SetEase(
          managers_.selection->keys(), ease)));
      Refresh();
    });
  }
  on_two_ = AddChip(tr("On 2s"), QStringLiteral("on_2s"));
  on_three_ = AddChip(tr("On 3s"), QStringLiteral("on_3s"));
  shorter_ = AddChip(tr("-1 f"), QStringLiteral("shorter"));
  longer_ = AddChip(tr("+1 f"), QStringLiteral("longer"));
  connect(on_two_, &QPushButton::clicked, this,
          [this] { RequestSpaceKeys(managers_, 2); });
  connect(on_three_, &QPushButton::clicked, this,
          [this] { RequestSpaceKeys(managers_, 3); });
  connect(shorter_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(
        managers_.keys->Hold(managers_.selection->keys(), -1)));
  });
  connect(longer_, &QPushButton::clicked, this, [this] {
    emit Problem(ProblemOf(
        managers_.keys->Hold(managers_.selection->keys(), 1)));
  });
  adjustSize();
  Refresh();
  assert(layout_.count() == 7);
}

QPushButton* KeyChipBar::AddChip(const QString& text, const QString& name) {
  assert(!text.isEmpty());
  assert(!name.isEmpty());
  // The layout's parent owns the chip from here.
  QPushButton* chip = std::make_unique<QPushButton>(text, this).release();
  chip->setObjectName(name);
  chip->setFocusPolicy(Qt::NoFocus);
  chip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  layout_.addWidget(chip);
  return chip;
}

void KeyChipBar::Refresh() {
  assert(managers_.keys != nullptr);
  assert(managers_.selection != nullptr);
  const std::set<KeyRef>& picked = managers_.selection->keys();
  const bool has_picked = !picked.empty();
  setVisible(has_picked);
  if (!has_picked) {
    return;
  }
  const std::optional<Ease> shared = managers_.keys->EaseOf(picked);
  for (size_t i = 0; i < kChipEases.size(); ++i) {
    const Ease ease = kChipEases[i];
    // Every eased in or out shape counts as the Ease chip.
    const bool is_shared = shared.has_value() && EaseText(*shared) ==
                                                     EaseText(ease);
    eases_[i]->setChecked(is_shared);
  }
  const QString why_no_space = managers_.keys->WhyNoSpace(picked);
  Explain(on_two_, why_no_space);
  Explain(on_three_, why_no_space);
  Explain(shorter_, managers_.keys->WhyNoHold(picked, -1),
          tr("Hold the last picked pose one frame shorter."));
  Explain(longer_, managers_.keys->WhyNoHold(picked, 1),
          tr("Hold the last picked pose one frame longer."));
}

QPoint KeyChipBar::SpotFor(const QRectF& keys, QSize bar,
                           const QRectF& area) {
  assert(bar.width() >= 0 && bar.height() >= 0);
  assert(area.width() >= 0.0);
  const double above = keys.top() - bar.height() - kGap;
  const bool has_room_above = above >= area.top();
  const double y = has_room_above ? above : keys.bottom() + kGap;
  const double left = keys.center().x() - bar.width() / 2.0;
  const double most_left = std::max(area.left(), area.right() - bar.width());
  const double x = std::clamp(left, area.left(), most_left);
  return QPoint(static_cast<int>(x), static_cast<int>(y));
}

}  // namespace snapper
