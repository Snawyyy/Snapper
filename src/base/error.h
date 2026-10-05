#ifndef SNAPPER_BASE_ERROR_H_
#define SNAPPER_BASE_ERROR_H_

#include <QString>

#include <expected>

namespace snapper {

// Why something failed, in words the user can act on.
struct Error final {
  QString message;
};

// Snapper has no exceptions: a call that can fail returns this.
template <typename T>
using Result = std::expected<T, Error>;

}  // namespace snapper

#endif  // SNAPPER_BASE_ERROR_H_
