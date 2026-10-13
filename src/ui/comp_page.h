#ifndef SNAPPER_UI_COMP_PAGE_H_
#define SNAPPER_UI_COMP_PAGE_H_

#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/grip_splitter.h"
#include "ui/comp_panel.h"
#include "ui/comp_stack.h"
#include "ui/comp_stage.h"
#include "ui/comp_timeline.h"
#include "ui/managers.h"
#include "ui/transport_bar.h"

class QKeyEvent;
class QShowEvent;

namespace snapper {

// The Comp tab: the effect stack on the left, the stage in the middle
// with the play controls and the Comp timeline under it, the picked
// effect's controls on the right. Its keys work while it shows: A adds,
// V held turns every effect off, K held shows what the picked one
// touches, I picks the color under the cursor as a target color,
// Delete removes, Ctrl+D duplicates, Shift+= and Shift+- step the
// blend mode.
class CompPage final : public QWidget {
  Q_OBJECT

 public:
  explicit CompPage(const Managers& managers);
  ~CompPage() override;

 signals:
  void Problem(const QString& why);

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private:
  void Report(const QString& problem) {
    if (!problem.isEmpty()) {
      emit Problem(problem);
    }
  }
  // True when key is one of the page's; is_press false for a release.
  bool IsPageKey(const QKeyEvent& key) const;
  void PressKey(const QKeyEvent& key);
  void ReleaseKey(const QKeyEvent& key);
  // Picks the shot on top at the reel playhead, as the effects belong to
  // it; the page works on the Video tab's reel, not on the Pose strip.
  void FollowPlayhead();

  Managers managers_;
  // Containers come before what they hold, so the held widgets leave
  // them before they go.
  QVBoxLayout layout_;
  GripSplitter split_;
  GripSplitter center_;
  QWidget bottom_;
  QVBoxLayout bottom_layout_;
  QScrollArea stack_scroll_;
  QScrollArea panel_scroll_;
  CompStack stack_;
  CompStage stage_;
  TransportBar transport_;
  CompTimeline timeline_;
  CompPanel panel_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_PAGE_H_
