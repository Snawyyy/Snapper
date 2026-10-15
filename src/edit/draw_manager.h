#ifndef SNAPPER_EDIT_DRAW_MANAGER_H_
#define SNAPPER_EDIT_DRAW_MANAGER_H_

#include <QColor>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QTransform>

#include <functional>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "edit/track_ref.h"
#include "model/project.h"

namespace snapper {

class HistoryManager;
class PaletteManager;
class PlaybackManager;

// What a new drawing starts from.
enum class DrawStart { kBlank, kShowing, kDefault };
// The tools of draw mode, each on its own key.
// kMove drags the whole drawing inside its frame, the piece's joint
// left where it is.
enum class DrawTool { kBrush, kEraser, kFill, kPicker, kMove };
inline constexpr int kDrawToolCount = 5;

// The most strokes draw mode can undo.
inline constexpr int kMaxDrawUndo = 64;

// What draw mode draws on: a drawing of a doll piece, or a cel of a
// storyboard layer (handed in by StoryboardManager as a DrawJob).
enum class DrawTarget { kPiece, kStoryboard };

// A picture another manager hands draw mode to draw on (a storyboard cel):
// the pixels to start from, what holding R shows, what lies dimmed
// under it and the onion skins, all in the picture's own pixels, and
// how Done keeps it.
struct DrawJob final {
  DrawTarget target = DrawTarget::kStoryboard;
  // "Storyboard: Shot 2, frame 12", for the title bar.
  QString title;
  QImage image;
  QImage reference;
  QImage under;
  std::vector<QImage> onions;
  // Done: keeps the drawing as one undo step, or says why not.
  std::function<Result<void>(const QImage&)> keep;
};

// Draw mode: drawing a new drawing for a doll piece, or a new version
// of one made in Snapper, without leaving Snapper; or drawing any
// picture a DrawJob hands in (a storyboard cel) with the same tools. It
// holds the drawing being made (the piece's size, the same for all its
// drawings), the tool, the brush size and the strokes to undo, all
// outside the project's history: Done saves the drawing as a PNG in the
// doll folder's "snapper" folder, lists it in the doll's rig and shows
// it on the piece at the frame drawing began, as one undo step (a job's
// picture goes to its keeper instead); Cancel leaves everything as it
// was. Playback pauses while drawing.
class DrawManager final : public QObject {
  Q_OBJECT

 public:
  // palette may be nullptr; then the brush color is the manager's own.
  DrawManager(HistoryManager* history, PlaybackManager* playback,
              PaletteManager* palette);

  // Why draw mode can't start on track (a doll piece) for a new drawing
  // or, with is_edit, for the drawing it shows; empty when it can.
  QString WhyNoDraw(const TrackRef& track, Frame frame, bool is_edit) const;
  // Starts a new drawing for track's piece from start.
  Result<void> Begin(const TrackRef& track, Frame frame, DrawStart start);
  // Starts on the drawing track's piece shows at frame: one made in
  // Snapper gets a new version in its place on Done; a Krita drawing is
  // copied into a new one, as Krita's next export would overwrite it.
  Result<void> BeginEdit(const TrackRef& track, Frame frame);
  // Starts on job's picture; Done hands the drawing to job.keep.
  Result<void> BeginJob(DrawJob job);

  bool is_drawing() const { return is_drawing_; }
  DrawTarget target() const { return target_; }
  // A piece's drawing is saved under a name; a job's picture is not.
  bool is_named() const { return target_ == DrawTarget::kPiece; }
  // What lies dimmed under a job's picture (null for a piece, whose
  // doll the canvas draws itself).
  const QImage& under() const { return under_; }
  // What the onion shows under the drawing at 30 %: the original for a
  // piece, the cels before and after for a storyboard.
  const std::vector<QImage>& onions() const { return onions_; }
  QString doll() const { return doll_; }
  QString piece() const { return track_.piece; }
  // "Drawing: mouth 2 (mouth)", for the title bar.
  QString Title() const;
  // What Done names a new drawing unless told otherwise ("mouth 2").
  QString DefaultName() const { return default_name_; }
  // True for an edit of a drawing made in Snapper.
  bool is_replacing() const { return replaces_ >= 0; }
  // The drawing being made, Format_ARGB32, the piece's size.
  const QImage& image() const { return image_; }
  // What it started from (for a blank start, the drawing shown), for
  // holding R and the onion.
  const QImage& reference() const { return reference_; }
  // The piece's drawing pixels to doll space at rest, where the doll's
  // other pieces stand.
  const QTransform& rest_place() const { return rest_place_; }
  // Something was drawn that Done would keep.
  bool is_dirty() const { return !undo_.empty(); }

  DrawTool tool() const { return tool_; }
  void SetTool(DrawTool tool);
  int brush_size() const { return brush_size_; }
  // Clamped to 1 to kMaxBrushSize.
  void SetBrushSize(int size);
  // The palette's current color, else the manager's own.
  QColor color() const;
  // Holds the original over the drawing while true (R held).
  bool is_peeking() const { return is_peeking_; }
  void SetPeeking(bool is_peeking);
  // The original under the drawing at 30 % (Shift+R).
  bool is_onion() const { return is_onion_; }
  void SetOnion(bool is_onion);
  // Flips step pages through the piece's drawings, as an animator flips
  // a stack: the canvas shows another of its drawings full strength in
  // place of the new one, round and round back to the new one. Drawing
  // goes back to the new one first. Only while drawing a piece.
  void Flip(int step);
  // A drawing other than the new one is shown (Flip).
  bool is_flipped() const { return flipped_ >= 0; }
  // The drawing shown while flipped.
  const QImage& flipped_image() const { return flipped_image_; }
  // The name of the drawing shown while flipped.
  const QString& flipped_name() const { return flipped_name_; }

  // A press at at (drawing pixels) with the current tool, or erasing
  // whatever the tool (the right button). With is_line, the brush or
  // eraser draws a straight line from the last point drawn.
  void Press(QPoint at, bool is_erase, bool is_line);
  // The mouse moved to at with the button down: the stroke goes on.
  void MoveTo(QPoint at);
  // The stroke ends.
  void Release();
  // Takes the color under at as the brush color.
  void PickColor(QPoint at);

  bool CanUndo() const { return !undo_.empty(); }
  bool CanRedo() const { return !redo_.empty(); }
  void Undo();
  void Redo();

  // Saves the drawing as name and leaves draw mode: one undo step,
  // "New drawing mouth 2" ("Edit drawing" for a new version). A job's
  // picture goes to its keep instead, name unused.
  Result<void> Done(const QString& name);
  // Leaves draw mode keeping nothing.
  void Cancel();

 signals:
  // Draw mode began or ended.
  void Started();
  void Ended();
  // The drawing, tool, size or color changed.
  void Changed();

 private:
  Result<void> Start(const TrackRef& track, Frame frame, int from,
                     int replaces);
  // Enters draw mode on the picture set up by Start or BeginJob.
  void Open();
  // Puts before (the drawing before a change) on the undo list.
  void Remember(const QImage& before);
  void End();
  // The project with the drawing at file listed for the piece, and,
  // for a new one, shown on it at the frame drawing began.
  Result<Project> WithDrawing(const QString& file, const QString& name) const;
  // A free file name for a new PNG in the doll's "snapper" folder.
  QString FreeFile(const QString& folder, const QString& name) const;

  HistoryManager* history_;
  PlaybackManager* playback_;
  PaletteManager* palette_;
  bool is_drawing_ = false;
  DrawTarget target_ = DrawTarget::kPiece;
  QImage under_;
  std::vector<QImage> onions_;
  std::function<Result<void>(const QImage&)> keep_;
  QString doll_;
  TrackRef track_;
  Frame frame_;
  // The drawing index Done puts a new version in, or -1 for a new one.
  int replaces_ = -1;
  QString default_name_;
  QString title_;
  // The piece's drawing shown in place of the new one, or -1 for the
  // new one (Flip).
  int flipped_ = -1;
  QImage flipped_image_;
  QString flipped_name_;
  // A Move drag: the drawing as it was when pressed, and where.
  bool is_moving_ = false;
  QImage move_base_;
  QPoint move_from_;
  QImage image_;
  QImage reference_;
  QTransform rest_place_;
  DrawTool tool_ = DrawTool::kBrush;
  int brush_size_ = 3;
  QColor own_color_ = Qt::black;
  bool is_peeking_ = false;
  bool is_onion_ = false;
  // Draw mode opened the palette, so it closes it again.
  bool did_open_palette_ = false;
  // The stroke in progress: its color and last point.
  bool is_stroking_ = false;
  QColor stroke_color_;
  QPoint last_;
  bool has_last_ = false;
  std::vector<QImage> undo_;
  std::vector<QImage> redo_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_DRAW_MANAGER_H_
