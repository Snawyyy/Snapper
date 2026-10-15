#ifndef SNAPPER_UI_FULL_VIEW_H_
#define SNAPPER_UI_FULL_VIEW_H_

#include <QPointer>
#include <QWidget>

#include <vector>

class QMainWindow;

namespace snapper {

// A widget with this property true stays shown in full view, floating
// over the view (the palette while drawing).
inline constexpr char kKeepInFullView[] = "keepInFullView";

// Tab's full view: one view (the stage, the reel preview) fills the
// screen. Entering hides everything beside it, up to the window, and
// the window's bars and docks, and puts the window full screen; leaving
// shows exactly what was hidden and puts the window back as it was.
class FullView final {
 public:
  explicit FullView(QMainWindow* window);
  // Leaves, so nothing stays hidden.
  ~FullView();
  FullView(const FullView&) = delete;
  FullView& operator=(const FullView&) = delete;

  bool is_on() const { return is_on_; }
  // Fills the screen with view, a widget inside the window.
  void Enter(QWidget* view);
  void Leave();

 private:
  // Hides widget, remembering it, when it shows.
  void Hide(QWidget* widget);

  QMainWindow* window_;
  bool is_on_ = false;
  bool was_full_screen_ = false;
  bool was_maximized_ = false;
  std::vector<QPointer<QWidget>> hidden_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_FULL_VIEW_H_
