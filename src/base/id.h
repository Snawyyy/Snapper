#ifndef SNAPPER_BASE_ID_H_
#define SNAPPER_BASE_ID_H_

#include <compare>
#include <cstddef>
#include <functional>

namespace snapper {

// A typed id, so a piece id can never be passed where a shot id is
// wanted. Zero means "none"; real ids start at 1 and are never reused.
template <typename Tag>
class Id final {
 public:
  constexpr Id() = default;
  constexpr explicit Id(int value) : value_(value) {}

  constexpr int value() const { return value_; }
  constexpr bool IsValid() const { return value_ > 0; }
  constexpr auto operator<=>(const Id&) const = default;

 private:
  int value_ = 0;
};

}  // namespace snapper

template <typename Tag>
struct std::hash<snapper::Id<Tag>> {
  std::size_t operator()(const snapper::Id<Tag>& id) const {
    return std::hash<int>()(id.value());
  }
};

#endif  // SNAPPER_BASE_ID_H_
