#include "ui/snapper_style.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QFrame>
#include <QPainter>
#include <QSplitterHandle>
#include <QStyleOption>
#include <QWidget>

#include <algorithm>
#include <cassert>
#include <memory>

#include "ui/crt_effect.h"
#include "ui/section_bar.h"
#include "ui/snapper_accents.h"
#include "ui/style_bevel.h"
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
// Pixels between ridges on a grip, and the most drawn on one.
constexpr int kRidgeGap = 4;
constexpr int kGripRidges = 5;

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
  const QRect face = option->rect.adjusted(0, 0, -1, -1);
  DrawBevel(painter, face, look.fill, look.is_sunk);
  // A toggled button (Play while playing) wears a red strip at its foot.
  const bool is_lit = is_enabled && is_checked;
  if (is_lit) {
    DrawAccentStrip(painter, QRect(face.left() + 2, face.bottom() - 3,
                                   face.width() - 3, 2));
  }
}

// A tab: raised when not picked and lifting under the pointer, sunken
// when picked, with a red strip over its top.
void DrawTabShape(const QStyleOption& option, const QWidget* widget,
                  QPainter* painter) {
  assert(painter != nullptr);
  assert(kTabLiftPx > 0);
  const bool is_picked = option.state.testFlag(QStyle::State_Selected);
  const double hover = HoverOf(&option, widget);
  const ButtonLook look = LookOfButton(true, false, is_picked, hover);
  const int lift = is_picked ? 0 : static_cast<int>(kTabLiftPx * hover);
  QRect rect = option.rect.adjusted(0, 0, -2, -1);
  rect.translate(0, -lift);
  DrawBevel(painter, rect, is_picked ? theme::kFaceLight.darker(125)
                                     : look.fill, is_picked);
  if (is_picked) {
    DrawAccentStrip(painter, QRect(rect.left() + 2, rect.top() + 2,
                                   rect.width() - 4, 3));
  }
}

// A splitter handle or dock separator: a thin groove in a wide grab area
// that turns into a yellow draggable handle under the pointer.
// Dock separators come with no hover state, so is_lit says it.
void DrawGrip(const QStyleOption* option, QPainter* painter, bool is_lit) {
  assert(option != nullptr);
  assert(painter != nullptr);
  const bool is_active =
      is_lit ||
      option->state.testFlags(QStyle::State_MouseOver | QStyle::State_Sunken);
  // Grips are the gaps between panels: the window shows through.
  painter->fillRect(option->rect, theme::kGap);
  DrawGripLine(painter, option->rect, is_active);
}

}  // namespace

void DrawGripLine(QPainter* painter, const QRect& box, bool is_active) {
  assert(painter != nullptr);
  assert(kRidgeGap > 0);
  const bool has_room = box.isValid();
  if (!has_room) {
    return;
  }
  // A hairline along the gap with a short knurled grip in its middle,
  // like the ridges on a cassette; all of it yellow while active.
  const bool is_bar = box.height() > box.width();
  const int along = is_bar ? box.height() : box.width();
  const int across = is_bar ? box.width() : box.height();
  const int middle = across / 2;
  const QRect line = is_bar ? QRect(box.left() + middle, box.top(), 1, along)
                            : QRect(box.left(), box.top() + middle, along, 1);
  painter->fillRect(line, is_active ? theme::kHandle : theme::kFaceDark);
  const int ridges = std::min(kGripRidges, along / kRidgeGap);
  const int ridge_length = std::max(2, across / 2);
  const int start = (across - ridge_length) / 2;
  const int first = (along - ridges * kRidgeGap) / 2;
  for (int i = 0; i < ridges; ++i) {
    const int at = first + i * kRidgeGap;
    const QRect ridge = is_bar ? QRect(box.left() + start, box.top() + at,
                                       ridge_length, 2)
                               : QRect(box.left() + at, box.top() + start, 2,
                                       ridge_length);
    painter->fillRect(ridge, is_active ? theme::kHandle : theme::kFaceLight);
  }
}

double SquashScale(double press) {
  assert(kSquash > 0.0 && kSquash < 0.5);
  assert(press > -100.0 && press < 100.0);
  return 1.0 - kSquash * std::clamp(press, -0.5, 1.0);
}

ButtonLook LookOfButton(bool is_enabled, bool is_down, bool is_checked,
                        double hover) {
  assert(hover >= 0.0 && hover <= 1.0);
  assert(theme::kRaised.isValid());
  ButtonLook look;
  // A greyed button lies flat on the panel; a live one stands raised.
  look.fill = is_enabled ? theme::kRaised : theme::kFace;
  const bool is_sunk = is_enabled && (is_down || is_checked);
  const bool is_reacting = is_enabled && !is_sunk;
  look.is_sunk = is_sunk;
  if (is_sunk) {
    look.fill = is_down ? theme::kFaceDark : theme::kFace.darker(110);
  } else if (is_reacting) {
    const int boost = static_cast<int>(kHoverBoostPercent * hover);
    look.fill = theme::kRaised.lighter(100 + boost);
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
  const bool is_check = element == PE_IndicatorCheckBox ||
                        element == PE_IndicatorRadioButton;
  if (is_check) {
    DrawCheckIndicator(painter, *option, element == PE_IndicatorRadioButton);
    return;
  }
  const bool is_focused_edit = element == PE_FrameLineEdit &&
                               option->state.testFlag(State_HasFocus);
  if (is_focused_edit) {
    QProxyStyle::drawPrimitive(element, option, painter, widget);
    DrawFocusRing(painter, option->rect);
    return;
  }
  const bool is_dock_grip = element == PE_IndicatorDockWidgetResizeHandle;
  if (is_dock_grip) {
    // DockGrips names the separator to light in the window's property.
    const QRect lit = widget != nullptr
                          ? widget->property("litGrip").toRect()
                          : QRect();
    DrawGrip(option, painter, !lit.isNull() && option->rect.intersects(lit));
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
    // A fold's header is a section bar: it wears the red strip too.
    const bool is_bar = widget != nullptr &&
                        widget->property("titlePlate").toBool();
    if (is_bar) {
      const QRect box = option->rect;
      DrawAccentStrip(painter, QRect(box.left() + 1, box.top() + 1, 3,
                                     box.height() - 2));
    }
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
  const bool is_splitter = element == CE_Splitter;
  if (is_splitter) {
    DrawGrip(option, painter, false);
    return;
  }
  const bool is_plate = element == CE_ShapedFrame && widget != nullptr &&
                        widget->property("titlePlate").toBool();
  if (is_plate) {
    QProxyStyle::drawControl(element, option, painter, widget);
    const QRect box = option->rect;
    DrawAccentStrip(painter, QRect(box.left() + 1, box.top() + 1, 3,
                                   box.height() - 2));
    return;
  }
  // A fold's header spans the panel; its arrow and title keep left.
  const auto* tool = qstyleoption_cast<const QStyleOptionToolButton*>(option);
  const bool is_bar_label = element == CE_ToolButtonLabel && tool != nullptr &&
                            widget != nullptr &&
                            widget->property("titlePlate").toBool();
  if (is_bar_label) {
    QStyleOptionToolButton left = *tool;
    left.rect.setLeft(left.rect.left() + 6);
    left.rect.setWidth(std::min(left.rect.width(),
                                widget->sizeHint().width()));
    QProxyStyle::drawControl(element, &left, painter, widget);
    return;
  }
  const bool is_tab_shape = element == CE_TabBarTabShape && tab != nullptr;
  const bool is_tab_label = element == CE_TabBarTabLabel && tab != nullptr;
  if (is_tab_shape) {
    DrawTabShape(*option, widget, painter);
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
  const auto* group = qstyleoption_cast<const QStyleOptionGroupBox*>(option);
  const bool is_section = control == CC_GroupBox && group != nullptr &&
                          !group->subControls.testFlag(SC_GroupBoxCheckBox);
  if (is_section) {
    DrawSection(painter, *group,
                widget != nullptr ? widget->font() : painter->font());
    return;
  }
  const auto* combo = qstyleoption_cast<const QStyleOptionComboBox*>(option);
  const bool is_combo = control == CC_ComboBox && combo != nullptr;
  if (!is_combo) {
    QProxyStyle::drawComplexControl(control, option, painter, widget);
    const bool is_focused_spin = control == CC_SpinBox &&
                                 option->state.testFlag(State_HasFocus);
    if (is_focused_spin) {
      DrawFocusRing(painter, option->rect);
    }
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
  const bool is_focused = combo->state.testFlag(State_HasFocus);
  if (is_focused) {
    DrawFocusRing(painter, combo->rect);
  }
}

int SnapperStyle::pixelMetric(PixelMetric metric, const QStyleOption* option,
                              const QWidget* widget) const {
  assert(kGripExtent > 0);
  assert(kGripExtent < 100);
  const bool is_grip = metric == PM_SplitterWidth ||
                       metric == PM_DockWidgetSeparatorExtent;
  if (is_grip) {
    return kGripExtent;
  }
  return QProxyStyle::pixelMetric(metric, option, widget);
}

QRect SnapperStyle::subControlRect(ComplexControl control,
                                  const QStyleOptionComplex* option,
                                  SubControl part,
                                  const QWidget* widget) const {
  assert(option != nullptr);
  assert(baseStyle() != nullptr);
  const auto* group = qstyleoption_cast<const QStyleOptionGroupBox*>(option);
  const bool is_section = control == CC_GroupBox && group != nullptr &&
                          !group->subControls.testFlag(SC_GroupBoxCheckBox);
  if (!is_section) {
    return QProxyStyle::subControlRect(control, option, part, widget);
  }
  return SectionPart(part, option->rect,
                     widget != nullptr ? widget->font() : QFont(),
                     IsSection(*group));
}

void SnapperStyle::polish(QWidget* widget) {
  assert(widget != nullptr);
  assert(baseStyle() != nullptr);
  QProxyStyle::polish(widget);
  // The handle lights up under the pointer only when it gets hover events.
  const bool is_handle = qobject_cast<QSplitterHandle*>(widget) != nullptr;
  if (is_handle) {
    widget->setAttribute(Qt::WA_Hover);
  }
  // Numbers show as on a little CRT.
  const bool is_number = qobject_cast<QAbstractSpinBox*>(widget) != nullptr;
  const bool needs_crt = is_number && widget->graphicsEffect() == nullptr;
  if (needs_crt) {
    // The widget owns it.
    widget->setGraphicsEffect(std::make_unique<CrtEffect>().release());
  }
  // Lists and grids are wells: a shade up from the fields, so what they
  // hold sits on a surface, not in a hole.
  auto* view = qobject_cast<QAbstractItemView*>(widget);
  const bool is_view = view != nullptr;
  if (is_view) {
    QPalette well = view->palette();
    well.setColor(QPalette::Base, theme::kWell);
    view->setPalette(well);
  }
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
