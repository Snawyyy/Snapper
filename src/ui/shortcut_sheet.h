#ifndef SNAPPER_UI_SHORTCUT_SHEET_H_
#define SNAPPER_UI_SHORTCUT_SHEET_H_

#include <QDialog>
#include <QMenuBar>
#include <QTableWidget>
#include <QVBoxLayout>

#include <array>
#include <vector>

#include "ui/command_palette.h"

namespace snapper {

// One thing a key or mouse gesture does that no menu lists.
struct Gesture final {
  const char* keys;
  const char* does;
};

// The gestures of the stage, timeline and rig canvas that are not menu
// entries: wheel, drags, nudge, guides.
inline constexpr std::array<Gesture, 22> kGestures = {{
    {"Click", "Pick what is under the cursor; Shift adds, Ctrl flips"},
    {"Drag on empty space", "Box pick; Shift adds, Ctrl takes out"},
    {"Drag a pick", "Move it; Shift locks to one axis, Ctrl scales"},
    {"Double-click", "Pick the whole doll or layer"},
    {"Mouse wheel", "Turn the pick 5 degrees (Shift: 1 degree)"},
    {"Ctrl + wheel", "Zoom the stage toward the cursor"},
    {"Middle button drag", "Slide the stage view"},
    {"Shift+1 / Shift+3 / Shift+2 / Shift+0",
     "Fit the frame / fit all / frame the pick / actual size"},
    {"O", "Show or hide what lies outside the frame"},
    {"Arrow keys", "Nudge the pick 1 pixel (Shift: 10)"},
    {"Tab / Shift+Tab", "Step the pick to the next or previous piece"},
    {"Escape", "Cancel a drag, drop the dot, then drop the pick"},
    {"D", "Make the picked dots drag behind (or stop)"},
    {"Hold P and drag", "Poke a doll to feel its physics"},
    {"Right-click", "Link, unlink and more for what is under the cursor"},
    {"Alt + drag a joint", "Pull a rope onto the part it hangs from"},
    {"Ctrl + drag a joint", "Place it freely, no snapping to the shape"},
    {"J", "Rig: joint circle on or off for the picked joint"},
    {"Drag a doll card", "Put the doll on the stage"},
    {"N / Shift+N", "Draw a new drawing for the picked piece / draw over "
                    "the one it shows"},
    {"Draw mode: B E G I, [ ], hold R, Shift+R",
     "Brush, eraser, fill, pick a color; brush size; the original; onion"},
    {"Shift+[ / Shift+]", "Doll picked: previous / next skin"},
}};

// Every key and gesture in one list: the menu entries that have a key,
// then the gestures. Opened from the Help menu (F1).
class ShortcutSheet final : public QDialog {
  Q_OBJECT

 public:
  ShortcutSheet(QMenuBar* bar, QWidget* parent);

  // Lists the keys as they are now and shows the sheet.
  void Open();

 private:
  QMenuBar* bar_;
  QVBoxLayout layout_;
  QTableWidget table_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHORTCUT_SHEET_H_
