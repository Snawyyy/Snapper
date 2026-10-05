#include "ui/theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

#include <cassert>

namespace snapper::theme {
namespace {

QPalette DarkPalette() {
  QPalette palette;
  for (const QPalette::ColorGroup group :
       {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    const bool is_off = group == QPalette::Disabled;
    palette.setColor(group, QPalette::Window, kFace);
    palette.setColor(group, QPalette::Button, kFace);
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
  }
  assert(palette.color(QPalette::Highlight) == kPick);
  return palette;
}

}  // namespace

void Apply(QApplication* application) {
  assert(application != nullptr);
  QStyle* style = QStyleFactory::create(QStringLiteral("Windows"));
  const bool has_style = style != nullptr;
  // Qt owns the style once set; Fusion is the fallback on odd builds.
  QApplication::setStyle(has_style ? style
                                   : QStyleFactory::create("Fusion"));
  QApplication::setPalette(DarkPalette());
  QFont font = QApplication::font();
  font.setPointSize(kFontPoints);
  QApplication::setFont(font);
  assert(QApplication::palette().color(QPalette::Highlight) == kPick);
}

}  // namespace snapper::theme
