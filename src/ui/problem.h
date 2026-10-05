#ifndef SNAPPER_UI_PROBLEM_H_
#define SNAPPER_UI_PROBLEM_H_

#include <QString>

#include "base/error.h"

namespace snapper {

// What a failed edit says, for the status bar; empty when it worked, so
// a success clears the last problem.
inline QString ProblemOf(const Result<void>& result) {
  return result.has_value() ? QString() : result.error().message;
}

// The same for edits that hand back a value.
template <typename T>
QString ProblemOf(const Result<T>& result) {
  return result.has_value() ? QString() : result.error().message;
}

}  // namespace snapper

#endif  // SNAPPER_UI_PROBLEM_H_
