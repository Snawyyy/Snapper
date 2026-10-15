#include "ui/theme.h"

#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>
#include <QWidget>

#include <cassert>
#include <memory>

#include "ui/snapper_style.h"

namespace snapper::theme {
namespace {

QPalette DarkPalette() {
  QPalette palette;
  assert(kFace.isValid());
  for (const QPalette::ColorGroup group :
       {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    const bool is_off = group == QPalette::Disabled;
    palette.setColor(group, QPalette::Window, kFace);
    palette.setColor(group, QPalette::Button, kRaised);
    // The Windows style builds its bevels from these four.
    palette.setColor(group, QPalette::Light, kFaceLight);
    palette.setColor(group, QPalette::Midlight, kFace.lighter(115));
    palette.setColor(group, QPalette::Dark, kFaceDark);
    palette.setColor(group, QPalette::Mid, kFaceDark);
    palette.setColor(group, QPalette::Shadow, kShadow);
    palette.setColor(group, QPalette::Base, kField);
    palette.setColor(group, QPalette::AlternateBase, kFaceDark);
    palette.setColor(group, QPalette::ToolTipBase, kFaceDark);
    palette.setColor(group, QPalette::ToolTipText, kText);
    palette.setColor(group, QPalette::Highlight,
                     is_off ? kFaceLight : kPick);
    palette.setColor(group, QPalette::HighlightedText, Qt::white);
    for (const QPalette::ColorRole role :
         {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
      palette.setColor(group, role, is_off ? kTextOff : kText);
    }
    palette.setColor(group, QPalette::Link, kPick.lighter(130));
    palette.setColor(group, QPalette::PlaceholderText, kTextHint);
  }
  assert(palette.color(QPalette::Highlight) == kPick);
  return palette;
}

}  // namespace

int Em(const QWidget* widget) {
  assert(widget != nullptr);
  const int em = QFontMetrics(widget->font()).height();
  assert(em > 0);
  return em;
}

void SetLevel(QWidget* widget, const QColor& level) {
  assert(widget != nullptr);
  assert(level.isValid());
  QPalette palette = widget->palette();
  palette.setColor(QPalette::Window, level);
  widget->setPalette(palette);
  widget->setAutoFillBackground(true);
}

void Apply(QApplication* application) {
  assert(application != nullptr);
  // Qt owns the style once set.
  QApplication::setStyle(std::make_unique<SnapperStyle>().release());
  QApplication::setPalette(DarkPalette());
  QFont font = QApplication::font();
  font.setPointSize(kFontPoints);
  QApplication::setFont(font);
  assert(QApplication::palette().color(QPalette::Highlight) == kPick);
}

}  // namespace snapper::theme
