#include "ui/collider_keys.h"

#include <cassert>

namespace snapper {

QString DotKey(const QString& piece, int slot) {
  assert(!piece.isEmpty());
  assert(slot >= 0);
  return piece + ":" + QString::number(slot);
}

std::optional<std::pair<QString, int>> ParseDotKey(const QString& key) {
  assert(key.size() < 100000);
  const qsizetype cut = key.lastIndexOf(':');
  bool is_number = false;
  const int slot = cut >= 0 ? key.mid(cut + 1).toInt(&is_number) : -1;
  const bool is_dot = cut > 0 && is_number && slot >= 0;
  assert(cut < key.size());
  return is_dot ? std::optional(std::pair(key.left(cut), slot))
                : std::nullopt;
}

}  // namespace snapper
