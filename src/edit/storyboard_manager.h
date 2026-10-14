#ifndef SNAPPER_EDIT_STORYBOARD_MANAGER_H_
#define SNAPPER_EDIT_STORYBOARD_MANAGER_H_

#include <QImage>
#include <QObject>
#include <QString>

#include <set>

#include "base/error.h"
#include "base/frame.h"
#include "edit/selection_manager.h"
#include "model/storyboard.h"
#include "model/shot.h"

namespace snapper {

class DocumentManager;
class DrawManager;
class HistoryManager;

// Each shot's draft (model/storyboard.h): one raster sketch made of cels, for
// planning the shot against its timing. Drawing a cel runs draw mode
// (DrawManager, with a DrawJob) on a picture the size of the canvas,
// over the shot as it is at that frame, with the cels before and after
// as onion skins; Done saves a new PNG in the draft folder and lists it
// as one undo step. Cels move and delete like keys, one step each; the
// eye and opacity are one step each too. The picked cels are view
// state, kept here so the timeline and the box agree.
class StoryboardManager final : public QObject {
  Q_OBJECT

 public:
  StoryboardManager(HistoryManager* history, DocumentManager* document,
               DrawManager* draw);

  // The shot's draft, or nullptr when the shot is gone.
  const Draft* Of(ShotId shot) const;
  // Where cels are saved: "<project> drafts" beside the project file,
  // or "drafts" in the app's data folder while the project is unsaved.
  QString Folder() const;

  // Why a cel can't be drawn in shot at frame (is_over: over the cel
  // showing there, else a new one starting there); empty when it can.
  QString WhyNoDraw(ShotId shot, Frame frame, bool is_over) const;
  // Draw mode on a new blank cel starting at frame; Done adds it, one
  // step "Draw draft cel".
  Result<void> DrawNew(ShotId shot, Frame frame);
  // Draw mode on the cel showing at frame; Done keeps the result as a
  // new version of that cel, one step "Edit draft cel".
  Result<void> DrawOver(ShotId shot, Frame frame);

  // The draft's eye, one step.
  Result<void> SetShown(ShotId shot, bool is_shown);
  // 0 to 1, one step (a fader drag folds into one by its scope).
  Result<void> SetOpacity(ShotId shot, double opacity);
  // Moves the cels starting at frames by offset frames, built on the
  // project the drag started from, so a drag calls it with the running
  // total. Refuses a cel landing before frame 0 or on a cel that stays.
  // The pick follows the cels.
  Result<void> MoveCels(ShotId shot, const std::set<Frame>& frames,
                        int offset);
  // Removes the cels starting at frames, one step.
  Result<void> DeleteCels(ShotId shot, const std::set<Frame>& frames);
  // Why the picked cels can't be removed; empty when they can.
  QString WhyNoDelete(ShotId shot) const;

  // The picked cels of shot (their start frames), only those that exist.
  std::set<Frame> Picked(ShotId shot) const;
  // Combines frames with the pick of shot by mode (a pick in another
  // shot is dropped first).
  void Pick(ShotId shot, const std::set<Frame>& frames, PickMode mode);
  void ClearPick();

 signals:
  // The pick changed.
  void Changed();

 private:
  // The picture of the cel at path at the canvas size, clear when it
  // can't be read.
  QImage CelImage(const QString& path) const;
  // A clear picture the size of the canvas.
  QImage Blank() const;
  // The shot at frame without its draft, for drawing over.
  QImage Under(const Shot& shot, Frame frame) const;
  // Saves image as a new PNG in Folder(); its path, or an error.
  Result<QString> Save(const QImage& image, ShotId shot, Frame frame) const;
  // Starts draw mode on a new cel starting at at, or (is_over) on the
  // cel showing there; Done saves it and lists it.
  Result<void> Start(ShotId shot, Frame at, bool is_over);

  HistoryManager* history_;
  DocumentManager* document_;
  DrawManager* draw_;
  ShotId picked_shot_;
  std::set<Frame> picked_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_STORYBOARD_MANAGER_H_
