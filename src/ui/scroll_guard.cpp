#include "ui/scroll_guard.h"

#include <QAbstractScrollArea>
#include <QDateTime>
#include <QScrollBar>

#include <cassert>

namespace snapper {
namespace {

// After a scroll, the wheel stays with the panel for this many ms.
constexpr qint64 kMidScrollMs = 600;
const char* const kScrolledAt = "snapper_scrolled_at";

}  // namespace

void WatchScrolling(QAbstractScrollArea* area) {
  assert(area != nullptr);
  assert(area->verticalScrollBar() != nullptr);
  QObject::connect(area->verticalScrollBar(), &QScrollBar::valueChanged, area,
                   [area] {
                     area->setProperty(
                         kScrolledAt, QDateTime::currentMSecsSinceEpoch());
                   });
}

bool IsMidScroll(const QWidget* widget) {
  assert(widget != nullptr);
  assert(kMidScrollMs > 0);
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  for (const QWidget* up = widget->parentWidget(); up != nullptr;
       up = up->parentWidget()) {
    const QVariant at = up->property(kScrolledAt);
    const bool is_watched = at.isValid();
    if (is_watched) {
      return now - at.toLongLong() < kMidScrollMs;
    }
  }
  return false;
}

}  // namespace snapper
