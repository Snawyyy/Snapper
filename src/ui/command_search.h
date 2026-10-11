#ifndef SNAPPER_UI_COMMAND_SEARCH_H_
#define SNAPPER_UI_COMMAND_SEARCH_H_

#include <QString>

#include <vector>

namespace snapper {

// Which of names the typed query finds, best first, as indices into
// names. A name matches when the query's letters come in it in order
// (case and spaces do not matter), so "svas" finds "Save as"; letters
// that start a word and letters side by side score higher, and a name
// that starts with the query comes first. An empty query finds every
// name in order.
std::vector<size_t> MatchCommands(const std::vector<QString>& names,
                                  const QString& query);

}  // namespace snapper

#endif  // SNAPPER_UI_COMMAND_SEARCH_H_
