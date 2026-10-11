#include "ui/command_search.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace snapper {
namespace {

// How well query's letters fit name, or -1 when they do not come in
// order.
int ScoreOf(const QString& name, const QString& query) {
  assert(!query.isEmpty());
  assert(name.size() < 100000);
  const QString lower = name.toLower();
  int score = 0;
  qsizetype from = 0;
  qsizetype last = -2;
  for (const QChar letter : query) {
    const qsizetype at = lower.indexOf(letter, from);
    const bool is_found = at >= 0;
    if (!is_found) {
      return -1;
    }
    const bool is_word_start = at == 0 || !lower[at - 1].isLetterOrNumber();
    const bool is_beside = at == last + 1;
    score += 1 + (is_word_start ? 8 : 0) + (is_beside ? 6 : 0);
    last = at;
    from = at + 1;
  }
  const bool is_prefix = lower.startsWith(query);
  return score + (is_prefix ? 40 : 0);
}

}  // namespace

std::vector<size_t> MatchCommands(const std::vector<QString>& names,
                                  const QString& query) {
  assert(names.size() < 1000000);
  assert(query.size() < 100000);
  const QString wanted = query.toLower().remove(' ');
  std::vector<std::pair<int, size_t>> scored;
  for (size_t i = 0; i < names.size(); ++i) {
    const int score = wanted.isEmpty() ? 0 : ScoreOf(names[i], wanted);
    const bool is_match = score >= 0;
    if (is_match) {
      scored.emplace_back(score, i);
    }
  }
  std::stable_sort(
      scored.begin(), scored.end(),
      [](const auto& a, const auto& b) { return a.first > b.first; });
  std::vector<size_t> found;
  for (const auto& entry : scored) {
    found.push_back(entry.second);
  }
  return found;
}

}  // namespace snapper
