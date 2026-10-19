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
#include "model/project.h"

namespace snapper {

class HistoryManager;
class PaletteManager;
class PlaybackManager;

// The tools of draw mode, each on its own key.
// kMove drags the whole drawing inside its frame.
enum class DrawTool { kBrush, kEraser, kFill, kPicker, kMove };
inline constexpr int kDrawToolCount = 5;

// The most strokes draw mode can undo.
inline constexpr int kMaxDrawUndo = 64;

// A picture another manager hands draw mode to draw on (a storyboard cel):
// the pixels to start from, what holding R shows, what lies dimmed
// under it and the onion skins, all in the picture's own pixels, and
// how Done keeps it.
struct DrawJob final {
  // "Storyboard: Shot 2, frame 12", for the title bar.
  QString title;
  QImage image;
  QImage reference;
  QImage under;
  std::vector<QImage> onions;
  // Done: keeps the drawing as one undo step, or says why not.
  std::function<Result<void>(const QImage&)> keep;
};

// Draw mode: drawing a picture a DrawJob hands in (a storyboard cel)
// with a pixel brush, eraser, fill, picker and move. It holds the
// picture, the tool, the brush size and the strokes to undo, all
// outside the project's history: Done hands the picture to the job's
// keeper (one undo step there); Cancel leaves everything as it was.
// Playback pauses while drawing.
class DrawManager final : public QObject {
  Q_OBJECT

 public:
  // palette may be nullptr; then the brush color is the manager's own.
  DrawManager(HistoryManager* history, PlaybackManager* playback,
              PaletteManager* palette);

  // Starts on job's picture; Done hands the drawing to job.keep.
  Result<void> BeginJob(DrawJob job);

  bool is_drawing() const { return is_drawing_; }
  // What lies dimmed under the picture.
  const QImage& under() const { return under_; }
  // What the onion shows under the drawing at 30 %: the cels before
  // and after.
  const std::vector<QImage>& onions() const { return onions_; }
  // "Storyboard: Shot 2, frame 12", for the title bar.
  const QString& Title() const { return title_; }
  // The drawing being made, Format_ARGB32.
  const QImage& image() const { return image_; }
  // The whole picture.
  QRect frame_rect() const { return image_.rect(); }
  // What it started from, for holding R.
  const QImage& reference() const { return reference_; }
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
  // The onion skins under the drawing at 30 % (Shift+R).
  bool is_onion() const { return is_onion_; }
  void SetOnion(bool is_onion);

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

  // Hands the drawing to the job's keeper and leaves draw mode.
  Result<void> Done();
  // Leaves draw mode keeping nothing.
  void Cancel();

 signals:
  // Draw mode began or ended.
  void Started();
  void Ended();
  // The drawing, tool, size or color changed.
  void Changed();

 private:
  // Enters draw mode on the picture set up by BeginJob.
  void Open();
  // Puts before (the drawing before a change) on the undo list.
  void Remember(const QImage& before);
  void End();

  HistoryManager* history_;
  PlaybackManager* playback_;
  PaletteManager* palette_;
  bool is_drawing_ = false;
  QImage under_;
  std::vector<QImage> onions_;
  std::function<Result<void>(const QImage&)> keep_;
  QString title_;
  // A Move drag: the drawing as it was when pressed, and where.
  bool is_moving_ = false;
  QImage move_base_;
  QPoint move_from_;
  QImage image_;
  QImage reference_;
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
