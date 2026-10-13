#ifndef SNAPPER_EDIT_REFERENCE_MANAGER_H_
#define SNAPPER_EDIT_REFERENCE_MANAGER_H_

#include <QImage>
#include <QObject>
#include <QPointF>
#include <QString>

#include <map>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "edit/video_file_frames.h"
#include "model/reference.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

// The two fit tiles of a reference.
enum class ReferenceFit { kFrame, kHeight };

// References on the Pose tab: videos, GIFs and pictures placed in a shot
// to animate over (model/reference.h). Every change to one is a named
// undo step through the history ("Reference opacity"); which one is
// picked and whether Y hides them all are view state, kept here and
// never saved. It also hands the stage each reference's picture of the
// moment, read through the same VideoFileFrames the Video tab uses, so a
// video stays open and plays forward decoding each picture once. Only
// the Pose tab stage draws them: nothing that renders the shot's own
// pictures knows about this manager.
class ReferenceManager final : public QObject {
  Q_OBJECT

 public:
  explicit ReferenceManager(HistoryManager* history);

  // Links the file at path into shot as a reference ("Add reference"),
  // fitted to the frame, behind everything at half opacity, starting on
  // frame 0. A still picture, an animated GIF or PNG, or any video
  // FFmpeg reads. It is picked; its id.
  Result<ReferenceId> Add(ShotId shot, const QString& path);
  Result<void> Remove(ShotId shot, ReferenceId id);
  // Points a reference whose file moved at its new place, keeping its
  // settings.
  Result<void> Relink(ShotId shot, ReferenceId id, const QString& path);

  Result<void> SetOpacity(ShotId shot, ReferenceId id, double opacity);
  Result<void> SetInFront(ShotId shot, ReferenceId id, bool is_in_front);
  Result<void> SetShown(ShotId shot, ReferenceId id, bool is_shown);
  Result<void> SetFlipped(ShotId shot, ReferenceId id, bool is_flipped);
  Result<void> SetTint(ShotId shot, ReferenceId id, ReferenceTint tint);
  // center in canvas pixels from the canvas middle; scale in canvas
  // pixels per picture pixel ("Move reference").
  Result<void> SetPlace(ShotId shot, ReferenceId id, QPointF center,
                        double scale);
  Result<void> Fit(ShotId shot, ReferenceId id, ReferenceFit fit);

  // The shot frame it starts on ("Reference start").
  Result<void> SetStart(ShotId shot, ReferenceId id, Frame start);
  // Seconds into the file shown at its start ("Slide reference").
  Result<void> SetSkip(ShotId shot, ReferenceId id, double skip);
  Result<void> SetSpeed(ShotId shot, ReferenceId id, double speed);
  // Marks the moment of the reference showing at shot frame local.
  Result<void> AddMark(ShotId shot, ReferenceId id, Frame local);
  QString WhyNoMark(ShotId shot, ReferenceId id, Frame local) const;
  Result<void> ClearMarks(ShotId shot, ReferenceId id);

  // The references of shot, empty when it is gone.
  std::vector<Reference> Of(ShotId shot) const;
  const Reference* Find(ShotId shot, ReferenceId id) const;
  // True when the file is not where the reference links it.
  static bool IsMissing(const Reference& reference);

  // The picked reference of shot, invalid for none. Picking one does not
  // touch the stage's pick of layers.
  ReferenceId PickedIn(ShotId shot) const;
  void Pick(ShotId shot, ReferenceId id);
  // Y: every reference shown or hidden at once, over their own eyes.
  bool are_shown() const { return are_shown_; }
  void ToggleShown();
  // True when reference is drawn at all now: its eye, Y, its file.
  bool IsVisible(const Reference& reference) const;

  // reference's picture at shot frame local; null when it shows nothing
  // then (before its start, hidden, or its file can't be read).
  QImage Picture(const Reference& reference, Frame local);

 signals:
  // The pick or Y changed; edits come through the history's Changed.
  void Changed();

 private:
  HistoryManager* history_;
  ShotId picked_shot_;
  ReferenceId picked_;
  bool are_shown_ = true;
  VideoFileFrames videos_;
  // Still pictures, read once each.
  std::map<QString, QImage> stills_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_REFERENCE_MANAGER_H_
