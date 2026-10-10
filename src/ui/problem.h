#ifndef SNAPPER_UI_PROBLEM_H_
#define SNAPPER_UI_PROBLEM_H_

#include <QString>

#include <cassert>

#include "base/error.h"

namespace snapper {

// What a failed edit says, for the status bar; empty when it worked, so
// a success clears the last problem.
inline QString ProblemOf(const Result<void>& result) {
  const QString problem =
      result.has_value() ? QString() : result.error().message;
  assert(!result.has_value() || problem.isEmpty());
  assert(result.has_value() || problem == result.error().message);
  return problem;
}

// The same for edits that hand back a value.
template <typename T>
QString ProblemOf(const Result<T>& result) {
  const QString problem =
      result.has_value() ? QString() : result.error().message;
  assert(!result.has_value() || problem.isEmpty());
  assert(result.has_value() || problem == result.error().message);
  return problem;
}

}  // namespace snapper

#endif  // SNAPPER_UI_PROBLEM_H_
