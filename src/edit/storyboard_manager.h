#ifndef SNAPPER_EDIT_STORYBOARD_MANAGER_H_
#define SNAPPER_EDIT_STORYBOARD_MANAGER_H_

#include <QImage>
#include <QObject>
#include <QString>

#include <set>

#include "base/error.h"
#include "base/frame.h"
#include "edit/selection_manager.h"
#include "model/project.h"
#include "model/shot.h"
#include "model/storyboard.h"

namespace snapper {

class DocumentManager;
class DrawManager;
class HistoryManager;

// The storyboard layers (model/storyboard.h): rough pictures drawn over
// a shot, one cel per stretch of time, so the storyboard moves through
// the shot. A storyboard layer is a layer of the shot's stack like any
// other (StageManager adds, orders, hides and removes it); this manager
// owns what is inside it.
//
// Drawing a cel runs draw mode (DrawManager, with a DrawJob) on a
// picture the size of the canvas, over the shot as it is at that frame,
// with the cel before as an onion skin to trace from. Done saves a new
// PNG beside the project and lists it as one undo step. With no
// storyboard layer yet, Done makes one in that same step. Cels move and
// delete like keys, one step each; opacity and the export tick are one
// step each too. The picked cels are view state, kept here so the
// timeline and the box agree.
class StoryboardManager final : public QObject {
  Q_OBJECT

 public:
  // data_folder holds the cel pictures of a project that was never
  // saved.
  StoryboardManager(HistoryManager* history, DocumentManager* document,
                    DrawManager* draw, SelectionManager* selection,
                    const QString& data_folder);

  // The storyboard layer an action in shot means: the one aimed at by a
  // click on its timeline row or its picked cels, else the picked layer
  // if it is one, else the topmost one. Invalid when the shot has none.
  LayerId Active(ShotId shot) const;
  // The storyboard layer the inspector shows: the one with picked cels,
  // else the picked layer if it is one. Invalid when neither is.
  LayerId PickedBoard(ShotId shot) const;
  // The storyboard inside layer of shot, or nullptr.
  const StoryboardLayer* Of(ShotId shot, LayerId layer) const;
  // Where cel pictures are saved: "<project> storyboard" beside the
  // project file, or "storyboard" in the data folder while unsaved.
  QString Folder() const;

  // Why a cel can't be drawn in shot at frame (is_over: over the cel
  // showing there, else a new one starting there); empty when it can.
  QString WhyNoDraw(ShotId shot, Frame frame, bool is_over) const;
  // Draw mode on a new blank cel starting at frame; Done adds it, one
  // step "Draw storyboard frame".
  Result<void> DrawNew(ShotId shot, Frame frame);
  // Draw mode on the cel showing at frame; Done keeps the result as a
  // new version of that cel, one step "Edit storyboard frame".
  Result<void> DrawOver(ShotId shot, Frame frame);

  // 0 to 1, one step (a fader drag folds into one by its scope).
  Result<void> SetOpacity(ShotId shot, LayerId layer, double opacity);
  // Whether the exported video shows the layer (an animatic), one step.
  Result<void> SetExported(ShotId shot, LayerId layer, bool is_exported);
  // How many limbs of the doll under it the layer sinks behind
  // (StoryboardLayer::sink), one step (a scrub folds into one).
  Result<void> SetSink(ShotId shot, LayerId layer, int sink);
  // Moves the cels starting at frames by offset frames, built on the
  // project the drag started from, so a drag calls it with the running
  // total. The pick follows the cels.
  Result<void> MoveCels(ShotId shot, LayerId layer,
                        const std::set<Frame>& frames, int offset);
  // Removes the cels starting at frames, one step.
  Result<void> DeleteCels(ShotId shot, LayerId layer,
                          const std::set<Frame>& frames);
  // Why the picked cels can't be removed; empty when they can.
  QString WhyNoDelete(ShotId shot) const;

  // The picked cels of layer (their start frames), only those that
  // exist.
  std::set<Frame> Picked(ShotId shot, LayerId layer) const;
  // Combines frames with the pick of layer by mode (a pick on another
  // layer is dropped first).
  void Pick(ShotId shot, LayerId layer, const std::set<Frame>& frames,
            PickMode mode);
  void ClearPick();

 signals:
  // The cel pick changed.
  void Changed();

 private:
  // A clear picture the size of the canvas.
  QImage Blank() const;
  // The picture of the cel at path at the canvas size, clear when it
  // can't be read.
  QImage CelImage(const QString& path) const;
  // The shot at frame with layer hidden, for drawing over.
  QImage Under(const Shot& shot, LayerId layer, Frame frame) const;
  // Saves image as a new PNG in Folder(); its path, or an error.
  Result<QString> Save(const QImage& image, const Shot& shot,
                       Frame frame) const;
  // Starts draw mode on a new cel starting at at, or (is_over) on the
  // cel showing there; Done saves it and lists it.
  Result<void> Start(ShotId shot, Frame at, bool is_over);
  // The project with the cel at path listed on layer at at, which is
  // made (on top, shown) when layer is not valid yet.
  Result<Project> WithCel(ShotId shot, LayerId layer, Frame at,
                          const QString& path, bool is_over) const;
  // Done: saves image and lists it in one step.
  Result<void> Keep(const QImage& image, ShotId shot, LayerId layer,
                    Frame at, bool is_over);

  HistoryManager* history_;
  DocumentManager* document_;
  DrawManager* draw_;
  SelectionManager* selection_;
  QString data_folder_;
  ShotId picked_shot_;
  LayerId picked_layer_;
  std::set<Frame> picked_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_STORYBOARD_MANAGER_H_
