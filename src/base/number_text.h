#ifndef SNAPPER_BASE_NUMBER_TEXT_H_
#define SNAPPER_BASE_NUMBER_TEXT_H_

#include <QString>

#include <optional>
#include <vector>

namespace snapper {

// Reads what someone typed into a number field. Plain numbers and sums
// ("90/2", "3*(2+1)", "-5") give their value. A text that starts with
// *, / or + works on current: "*2" doubles it, "+15" adds 15. Anything
// else, a divide by zero or a result that is not finite gives nothing,
// so a bad entry never half-applies.
std::optional<double> EvalNumber(const QString& text, double current);

// True when picked values are not all the same, so a field that shows
// one number for them all would lie and should say "Mixed".
bool HasMixedValues(const std::vector<double>& values);

}  // namespace snapper

#endif  // SNAPPER_BASE_NUMBER_TEXT_H_
