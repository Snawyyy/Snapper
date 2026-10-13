#include "ui/snapper_style.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFrame>
#include <QPainter>
#include <QStyleOption>
#include <QWidget>

#include <algorithm>
#include <cassert>

#include "ui/theme.h"

namespace snapper {
namespace {

// A hovered button is at most this much lighter, in percent.
constexpr int kHoverBoostPercent = 16;
// A chunky tab is padded this much beyond its text.
constexpr int kTabPadX = 18;
constexpr int kTabPadY = 8;
// A hovered tab rises this many pixels.
constexpr int kTabLiftPx = 1;

double HoverOf(const QStyleOption* option, const QWidget* widget) {
  assert(option != nullptr);
  assert(kHoverProperty[0] != '\0');
  const bool is_over = option->state.testFlag(QStyle::State_MouseOver);
  const QVariant faded =
      widget != nullptr ? widget->property(kHoverProperty) : QVariant();
  const bool is_fading = faded.isValid();
  return is_fading ? std::clamp(faded.toDouble(), 0.0, 1.0)
                   : (is_over ? 1.0 : 0.0);
}

// A one pixel bevel: light on the top and left, shadow on the bottom and
// right, or the other way round when sunk.
void DrawBevel(QPainter* painter, const QRect& rect, const QColor& fill,
               bool is_sunk) {
  assert(painter != nullptr);
  assert(rect.isValid());
  painter->fillRect(rect, fill);
  const QColor top_left = is_sunk ? theme::kShadow : theme::kFaceLight;
  const QColor bottom_right = is_sunk ? theme::kFaceLight : theme::kShadow;
  painter->setPen(top_left);
  painter->drawLine(rect.topLeft(), rect.topRight());
  painter->drawLine(rect.topLeft(), rect.bottomLeft());
  painter->setPen(bottom_right);
  painter->drawLine(rect.bottomLeft(), rect.bottomRight());
  painter->drawLine(rect.topRight(), rect.bottomRight());
  const bool is_raised = !is_sunk;
  if (is_raised) {
    const QRect inner = rect.adjusted(1, 1, -1, -1);
    painter->setPen(theme::kFaceDark);
    painter->drawLine(inner.bottomLeft(), inner.bottomRight());
    painter->drawLine(inner.topRight(), inner.bottomRight());
  }
}

// A square sunken frame with no fill: shadow on the top and left, light
// on the bottom and right. Group boxes are drawn so, like the frames of
// the panel kit.
void DrawSunkenFrame(QPainter* painter, const QRect& rect) {
  assert(painter != nullptr);
  assert(rect.isValid());
  painter->save();
  painter->setPen(theme::kShadow);
  painter->drawLine(rect.topLeft(), rect.topRight());
  painter->drawLine(rect.topLeft(), rect.bottomLeft());
  painter->setPen(theme::kFaceLight);
  painter->drawLine(rect.bottomLeft(), rect.bottomRight());
  painter->drawLine(rect.topRight(), rect.bottomRight());
  painter->restore();
}

void DrawButton(const QStyleOption* option, QPainter* painter,
                const QWidget* widget) {
  assert(option != nullptr);
  assert(painter != nullptr);
  const QStyle::State state = option->state;
  const bool is_enabled = state.testFlag(QStyle::State_Enabled);
  const bool is_down = state.testFlag(QStyle::State_Sunken);
  const bool is_checked = state.testFlag(QStyle::State_On);
  const ButtonLook look = LookOfButton(is_enabled, is_down, is_checked,
                                       HoverOf(option, widget));
  DrawBevel(painter, option->rect.adjusted(0, 0, -1, -1), look.fill,
            look.is_sunk);
}

}  // namespace

double SquashScale(double press) {
  assert(kSquash > 0.0 && kSquash < 0.5);
  assert(press > -100.0 && press < 100.0);
  return 1.0 - kSquash * std::clamp(press, -0.5, 1.0);
}

ButtonLook LookOfButton(bool is_enabled, bool is_down, bool is_checked,
                        double hover) {
  assert(hover >= 0.0 && hover <= 1.0);
  assert(theme::kFace.isValid());
  ButtonLook look;
  look.fill = theme::kFace;
  const bool is_sunk = is_enabled && (is_down || is_checked);
  const bool is_reacting = is_enabled && !is_sunk;
  look.is_sunk = is_sunk;
  if (is_sunk) {
    look.fill = is_down ? theme::kFaceDark : theme::kFace.darker(118);
  } else if (is_reacting) {
    const int boost = static_cast<int>(kHoverBoostPercent * hover);
    look.fill = theme::kFace.lighter(100 + boost);
  }
  return look;
}

SnapperStyle::SnapperStyle() : QProxyStyle(QStringLiteral("windows")) {
  assert(baseStyle() != nullptr);
  assert(baseStyle()->name() == QStringLiteral("windows"));
}

void SnapperStyle::drawPrimitive(PrimitiveElement element,
                                 const QStyleOption* option,
                                 QPainter* painter,
                                 const QWidget* widget) const {
  assert(option != nullptr);
  assert(painter != nullptr);
  const bool is_button = element == PE_PanelButtonCommand ||
                         element == PE_PanelButtonTool;
  const bool is_flat_tool =
      element == PE_PanelButtonTool &&
      !option->state.testFlags(State_Sunken | State_On | State_MouseOver) &&
      option->state.testFlag(State_AutoRaise);
  if (is_flat_tool) {
    return;
  }
  const bool is_group_frame =
      element == PE_FrameGroupBox && option->rect.isValid();
  if (is_group_frame) {
    DrawSunkenFrame(painter, option->rect.adjusted(0, 0, -1, -1));
    return;
  }
  if (is_button) {
    DrawButton(option, painter, widget);
    return;
  }
  QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void SnapperStyle::drawControl(ControlElement element,
                               const QStyleOption* option, QPainter* painter,
                               const QWidget* widget) const {
  assert(option != nullptr);
  assert(painter != nullptr);
  const auto* tab = qstyleoption_cast<const QStyleOptionTab*>(option);
  const bool is_tab_shape = element == CE_TabBarTabShape && tab != nullptr;
  const bool is_tab_label = element == CE_TabBarTabLabel && tab != nullptr;
  if (is_tab_shape) {
    const bool is_picked = tab->state.testFlag(State_Selected);
    const double hover = HoverOf(option, widget);
    const ButtonLook look = LookOfButton(true, false, is_picked, hover);
    const int lift = is_picked ? 0 : static_cast<int>(kTabLiftPx * hover);
    QRect rect = option->rect.adjusted(0, 0, -2, -1);
    rect.translate(0, -lift);
    DrawBevel(painter, rect, is_picked ? theme::kFaceLight.darker(125)
                                       : look.fill, is_picked);
    return;
  }
  const bool is_push = element == CE_PushButton && widget != nullptr;
  const double press =
      is_push ? widget->property(kPressProperty).toDouble() : 0.0;
  const bool is_squashed = is_push && press != 0.0;
  if (is_squashed) {
    const QPointF middle = QRectF(option->rect).center();
    painter->save();
    painter->translate(middle);
    painter->scale(SquashScale(press), SquashScale(press));
    painter->translate(-middle);
    QProxyStyle::drawControl(element, option, painter, widget);
    painter->restore();
    return;
  }
  if (is_tab_label) {
    QStyleOptionTab lit = *tab;
    const bool is_picked = tab->state.testFlag(State_Selected);
    const QColor ink = is_picked ? QColor(Qt::white) : theme::kText;
    lit.palette.setColor(QPalette::WindowText, ink);
    lit.palette.setColor(QPalette::ButtonText, ink);
    QProxyStyle::drawControl(element, &lit, painter, widget);
    return;
  }
  QProxyStyle::drawControl(element, option, painter, widget);
}

void SnapperStyle::drawComplexControl(ComplexControl control,
                                      const QStyleOptionComplex* option,
                                      QPainter* painter,
                                      const QWidget* widget) const {
  assert(option != nullptr);
  assert(painter != nullptr);
  const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option);
  const bool is_combo = control == CC_ComboBox && combo != nullptr;
  if (!is_combo) {
    QProxyStyle::drawComplexControl(control, option, painter, widget);
    return;
  }
  const bool is_enabled = combo->state.testFlag(State_Enabled);
  // The box sinks into the panel; its arrow is a small raised button.
  DrawBevel(painter, combo->rect.adjusted(0, 0, -1, -1),
            is_enabled ? theme::kField : theme::kFace, true);
  const QRect arrow = subControlRect(CC_ComboBox, combo, SC_ComboBoxArrow,
                                     widget)
                          .adjusted(1, 1, -2, -2);
  const bool is_down = combo->state.testFlag(State_On);
  const ButtonLook look =
      LookOfButton(is_enabled, is_down, false, HoverOf(option, widget));
  DrawBevel(painter, arrow, look.fill, look.is_sunk);
  QStyleOption mark = *option;
  mark.rect = arrow;
  drawPrimitive(PE_IndicatorArrowDown, &mark, painter, widget);
  QStyleOptionComboBox text = *combo;
  text.palette.setColor(QPalette::ButtonText, theme::kText);
  text.palette.setColor(QPalette::Text, theme::kText);
  drawControl(CE_ComboBoxLabel, &text, painter, widget);
}

void SnapperStyle::polish(QWidget* widget) {
  assert(widget != nullptr);
  assert(baseStyle() != nullptr);
  QProxyStyle::polish(widget);
  auto* combo = qobject_cast<QComboBox*>(widget);
  const bool is_combo = combo != nullptr;
  if (!is_combo) {
    return;
  }
  combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  // The list it opens is sunken too, on the field colour.
  QAbstractItemView* list = combo->view();
  list->setFrameStyle(QFrame::Panel | QFrame::Sunken);
  QPalette field = list->palette();
  field.setColor(QPalette::Base, theme::kField);
  list->setPalette(field);
}

QSize SnapperStyle::sizeFromContents(ContentsType type,
                                     const QStyleOption* option,
                                     const QSize& size,
                                     const QWidget* widget) const {
  assert(option != nullptr);
  assert(type >= CT_PushButton);
  const QSize base = QProxyStyle::sizeFromContents(type, option, size, widget);
  const bool is_tab = type == CT_TabBarTab;
  return is_tab ? base + QSize(kTabPadX, kTabPadY) : base;
}

}  // namespace snapper
