#include "ui/comp_drag.h"

#include <QPainter>
#include <QPixmap>

#include <cassert>

namespace snapper {
namespace {

const char* const kEntryType = "application/x-snapper-comp-entry";
const char* const kEffectType = "application/x-snapper-comp-effect";
constexpr int kMarkSize = 12;

}  // namespace

std::unique_ptr<QMimeData> EntryMime(const CompEntry& entry) {
  assert(static_cast<int>(entry.kind) >= 0);
  assert(static_cast<int>(entry.kind) < kCompKindCount);
  auto mime = std::make_unique<QMimeData>();
  const QString text = QStringLiteral("%1\n%2\n%3")
                           .arg(static_cast<int>(entry.kind))
                           .arg(entry.is_scene_gradient ? 1 : 0)
                           .arg(entry.plugin);
  mime->setData(QLatin1String(kEntryType), text.toUtf8());
  return mime;
}

std::optional<CompEntry> EntryOf(const QMimeData* mime) {
  assert(kCompKindCount > 0);
  const bool has_entry =
      mime != nullptr && mime->hasFormat(QLatin1String(kEntryType));
  if (!has_entry) {
    return std::nullopt;
  }
  const QStringList parts =
      QString::fromUtf8(mime->data(QLatin1String(kEntryType))).split('\n');
  const bool is_whole = parts.size() == 3;
  if (!is_whole) {
    return std::nullopt;
  }
  bool is_number = false;
  const int kind = parts[0].toInt(&is_number);
  const bool is_kind = is_number && kind >= 0 && kind < kCompKindCount;
  if (!is_kind) {
    return std::nullopt;
  }
  CompEntry entry;
  entry.kind = static_cast<CompKind>(kind);
  entry.is_scene_gradient = parts[1] == QStringLiteral("1");
  entry.plugin = parts[2];
  assert(static_cast<int>(entry.kind) < kCompKindCount);
  return entry;
}

std::unique_ptr<QMimeData> EffectMime(CompEffectId id) {
  assert(id.IsValid());
  assert(id.value() >= 0);
  auto mime = std::make_unique<QMimeData>();
  mime->setData(QLatin1String(kEffectType), QByteArray::number(id.value()));
  return mime;
}

std::optional<CompEffectId> EffectOf(const QMimeData* mime) {
  assert(kEffectType != nullptr);
  const bool has_effect =
      mime != nullptr && mime->hasFormat(QLatin1String(kEffectType));
  if (!has_effect) {
    return std::nullopt;
  }
  bool is_number = false;
  const int value = mime->data(QLatin1String(kEffectType)).toInt(&is_number);
  const bool is_id = is_number && value >= 0;
  assert(!is_id || value >= 0);
  return is_id ? std::optional(CompEffectId(value)) : std::nullopt;
}

QColor KindMark(CompKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kCompKindCount);
  switch (kind) {
    case CompKind::kLineSmoother:
      return QColor(0x8a, 0x8a, 0x90);
    case CompKind::kColorSwap:
      return QColor(0xc0, 0x5a, 0xd0);
    case CompKind::kTransmittedLight:
      return QColor(0xf0, 0xb0, 0x50);
    case CompKind::kRimLight:
      return QColor(0xff, 0xe8, 0xb0);
    case CompKind::kGradient:
      return QColor(0x50, 0x90, 0xe0);
    case CompKind::kMotionBlur:
      return QColor(0x60, 0xc0, 0xa0);
    case CompKind::kSparkle:
      return QColor(0xff, 0xff, 0xff);
    case CompKind::kSpeedLines:
      return QColor(0x40, 0x40, 0x40);
    case CompKind::kFinish:
      return QColor(0xa0, 0x80, 0x60);
    case CompKind::kFsp:
      return QColor(0xe0, 0x70, 0x40);
  }
  return Qt::gray;
}

QIcon MarkIcon(CompKind kind) {
  assert(static_cast<int>(kind) < kCompKindCount);
  assert(kMarkSize > 2);
  QPixmap mark(kMarkSize, kMarkSize);
  mark.fill(Qt::transparent);
  QPainter painter(&mark);
  painter.setPen(Qt::black);
  painter.setBrush(KindMark(kind));
  painter.drawRect(0, 0, kMarkSize - 1, kMarkSize - 1);
  return QIcon(mark);
}

}  // namespace snapper
