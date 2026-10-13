#ifndef SNAPPER_UI_GOLDEN_SPLITTER_H_
#define SNAPPER_UI_GOLDEN_SPLITTER_H_

#include <QString>

class QSplitter;
class QWidget;

namespace snapper {

// Splitters that share their length by the golden ratio and come back
// as they were left.

// Two panes: the one at long_index takes the golden ratio's long part,
// the other the short part, and a resize keeps the ratio.
void SplitGolden(QSplitter* splitter, int long_index);

// Three panes: the centre one takes the long part and the two beside it
// share the short part equally.
void SplitGoldenAround(QSplitter* splitter);

// Puts every splitter under root that has an object name back as it was
// left (from the settings file, under key/<name>), and saves them all
// there when root's window closes. An empty settings_path remembers
// nothing.
void RememberSplits(QWidget* root, const QString& settings_path,
                    const QString& key);

}  // namespace snapper

#endif  // SNAPPER_UI_GOLDEN_SPLITTER_H_
