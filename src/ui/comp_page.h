#ifndef SNAPPER_UI_COMP_PAGE_H_
#define SNAPPER_UI_COMP_PAGE_H_

#include <QHBoxLayout>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/comp_effects.h"
#include "ui/comp_library.h"
#include "ui/comp_stage.h"
#include "ui/comp_timeline.h"
#include "ui/deck_panel.h"
#include "ui/grip_splitter.h"
#include "ui/managers.h"
#include "ui/transport_bar.h"

class QKeyEvent;
class QShowEvent;

namespace snapper {

// The Comp tab: every effect on the left, the stage in the middle with
// the play controls and the Comp timeline under it, the effects on
// what is picked on the stage on the right. Its keys work while it
// shows: A searches the effects, S splits the picked clips at the
// playhead, Ctrl+C, Ctrl+X and Ctrl+V copy, cut and paste them (at the
// playhead), V held turns every effect off, K held shows what the
// picked one touches, I picks the color under the cursor as a target
// color, Delete removes, Ctrl+D duplicates, Shift+= and Shift+- step
// the blend mode.
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
    const bool has_problem = !problem.isEmpty();
    if (has_problem) {
      emit Problem(problem);
    }
  }
  // True when key is one of the page's; is_press false for a release.
  bool IsPageKey(const QKeyEvent& key) const;
  void PressKey(const QKeyEvent& key);
  // The clip keys (S, Ctrl+C, X, V and D, Delete) on the timeline's
  // picked clips; false when key is not one.
  bool PressClipKey(const QKeyEvent& key);
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
  // The deck panel strip the play controls sit on.
  DeckPanel deck_;
  QHBoxLayout deck_layout_;
  QScrollArea effects_scroll_;
  CompLibrary library_;
  CompStage stage_;
  TransportBar transport_;
  CompTimeline timeline_;
  CompEffects effects_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_COMP_PAGE_H_
