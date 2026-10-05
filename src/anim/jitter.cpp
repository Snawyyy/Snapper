#include "anim/jitter.h"

#include <cassert>
#include <cstdint>

namespace snapper {

double Jitter(int step, int axis) {
  assert(step >= 0);
  assert(axis >= 0 && axis < 64);
  // xorshift then a multiplicative hash: cheap and well spread.
  auto x = static_cast<std::uint32_t>(step) * 64u +
           static_cast<std::uint32_t>(axis) + 1u;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  x *= 2654435761u;
  return (x % 2001u) / 1000.0 - 1.0;
}

}  // namespace snapper
