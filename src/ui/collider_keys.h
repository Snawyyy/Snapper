#ifndef SNAPPER_UI_COLLIDER_KEYS_H_
#define SNAPPER_UI_COLLIDER_KEYS_H_

#include <QString>

#include <optional>
#include <utility>

namespace snapper {

// The name a collider dot goes by in picks and hovers: its piece and
// its slot on the ring.
QString DotKey(const QString& piece, int slot);

// The piece and slot a key names, or nothing for a key that isn't one.
std::optional<std::pair<QString, int>> ParseDotKey(const QString& key);

}  // namespace snapper

#endif  // SNAPPER_UI_COLLIDER_KEYS_H_
