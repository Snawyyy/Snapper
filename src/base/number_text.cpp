#include "base/number_text.h"

#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// Longer entries are not math anyone types into a field.
constexpr int kMaxLength = 64;
// Marks a minus that negates one value instead of subtracting two.
constexpr QChar kNegate = QLatin1Char('n');
constexpr QChar kOpen = QLatin1Char('(');

// Values and the operators waiting for their second value.
struct Calc final {
  std::vector<double> values;
  std::vector<QChar> ops;
};

int Weight(QChar op) {
  assert(!op.isNull());
  assert(op != QLatin1Char(')'));
  const bool is_negate = op == kNegate;
  const bool is_times = op == QLatin1Char('*') || op == QLatin1Char('/');
  const bool is_plus = op == QLatin1Char('+') || op == QLatin1Char('-');
  return is_negate ? 3 : is_times ? 2 : is_plus ? 1 : 0;
}

// Does the operator on top of the stack, using the values it needs.
bool Reduce(Calc* calc) {
  assert(calc != nullptr);
  assert(!calc->ops.empty());
  const QChar op = calc->ops.back();
  calc->ops.pop_back();
  const bool is_negate = op == kNegate;
  const bool has_one = !calc->values.empty();
  const bool can_negate = is_negate && has_one;
  if (can_negate) {
    calc->values.back() = -calc->values.back();
    return true;
  }
  const bool has_two = !is_negate && calc->values.size() >= 2;
  if (!has_two) {
    return false;
  }
  const double right = calc->values.back();
  calc->values.pop_back();
  double* left = &calc->values.back();
  const bool is_divide_by_zero = op == QLatin1Char('/') && right == 0.0;
  if (is_divide_by_zero) {
    return false;
  }
  *left = op == QLatin1Char('+')   ? *left + right
          : op == QLatin1Char('-') ? *left - right
          : op == QLatin1Char('*') ? *left * right
                                   : *left / right;
  return true;
}

// Reduces while the top operator binds at least as hard as weight; an
// open bracket stops it.
bool ReduceWhile(Calc* calc, int weight) {
  assert(calc != nullptr);
  assert(weight >= 0);
  const int bound = static_cast<int>(calc->ops.size());
  for (int i = 0; i < bound; ++i) {
    const bool is_stopped =
        calc->ops.empty() || calc->ops.back() == kOpen ||
        Weight(calc->ops.back()) < weight;
    if (is_stopped) {
      return true;
    }
    const bool is_reduced = Reduce(calc);
    if (!is_reduced) {
      return false;
    }
  }
  return true;
}

// Reads the number starting at text[*at]; moves *at past it.
std::optional<double> ReadNumber(const QString& text, int* at) {
  assert(at != nullptr);
  assert(*at >= 0 && *at < text.size());
  const int first = *at;
  int end = first;
  while (end < text.size() &&
         (text[end].isDigit() || text[end] == QLatin1Char('.'))) {
    ++end;
  }
  *at = end;
  bool is_number = false;
  const double value = text.mid(first, end - first).toDouble(&is_number);
  return is_number ? std::optional<double>(value) : std::nullopt;
}

// Closes a bracket: finishes what is inside it.
bool Close(Calc* calc) {
  assert(calc != nullptr);
  assert(calc->values.size() < 100000);
  const bool is_reduced = ReduceWhile(calc, 0);
  const bool is_inside = is_reduced && !calc->ops.empty();
  if (is_inside) {
    calc->ops.pop_back();
  }
  return is_inside;
}

// Folds one character (or number) in; false when the text is wrong.
bool Feed(const QString& text, int* at, bool* wants_value, Calc* calc) {
  assert(at != nullptr && wants_value != nullptr);
  assert(calc != nullptr);
  const QChar c = text[*at];
  const bool is_number_start = c.isDigit() || c == QLatin1Char('.');
  if (is_number_start) {
    const auto value = ReadNumber(text, at);
    const bool can_take = value.has_value() && *wants_value;
    if (can_take) {
      calc->values.push_back(*value);
      *wants_value = false;
    }
    return can_take;
  }
  ++*at;
  const bool is_sign = c == QLatin1Char('+') || c == QLatin1Char('-');
  const bool is_binary = !*wants_value && (is_sign || c == QLatin1Char('*') ||
                                           c == QLatin1Char('/'));
  const bool is_open = c == kOpen && *wants_value;
  const bool is_close = c == QLatin1Char(')') && !*wants_value;
  const bool is_unary = is_sign && *wants_value;
  const bool is_negation = is_unary && c == QLatin1Char('-');
  if (is_open) {
    calc->ops.push_back(c);
  } else if (is_negation) {
    calc->ops.push_back(kNegate);
  } else if (is_binary) {
    *wants_value = true;
    const bool is_ready = ReduceWhile(calc, Weight(c));
    calc->ops.push_back(c);
    return is_ready;
  }
  return is_close ? Close(calc) : is_open || is_unary;
}

}  // namespace

std::optional<double> EvalNumber(const QString& text, double current) {
  assert(std::isfinite(current));
  assert(kMaxLength > 0);
  QString typed = text;
  typed.remove(QLatin1Char(' ')).replace(QLatin1Char(','), QLatin1Char('.'));
  const bool is_usable = !typed.isEmpty() && typed.size() <= kMaxLength;
  if (!is_usable) {
    return std::nullopt;
  }
  const bool is_open = typed.startsWith(QLatin1Char('*')) ||
                       typed.startsWith(QLatin1Char('/')) ||
                       typed.startsWith(QLatin1Char('+'));
  if (is_open) {
    typed.prepend(kOpen + QString::number(current, 'g', 17) +
                  QLatin1Char(')'));
  }
  Calc calc;
  bool wants_value = true;
  int at = 0;
  for (int step = 0; step < typed.size() && at < typed.size(); ++step) {
    const bool is_fed = Feed(typed, &at, &wants_value, &calc);
    if (!is_fed) {
      return std::nullopt;
    }
  }
  const bool is_closed = ReduceWhile(&calc, 0) && calc.ops.empty();
  const bool is_one = calc.values.size() == 1;
  const bool is_good =
      !wants_value && is_closed && is_one && std::isfinite(calc.values[0]);
  return is_good ? std::optional<double>(calc.values[0]) : std::nullopt;
}

bool HasMixedValues(const std::vector<double>& values) {
  assert(values.size() < 100000);
  assert(values.empty() || std::isfinite(values.front()));
  for (const double value : values) {
    const bool is_other = value != values.front();
    if (is_other) {
      return true;
    }
  }
  return false;
}

}  // namespace snapper
