#include "edit/draw_manager.h"

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "anim/doll_pose.h"
#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "edit/playback_manager.h"
#include "edit/project_edits.h"
#include "render/pixel_paint.h"

namespace snapper {
namespace {

// The drawing index drawing (-1 for the default) stands for on piece.
int ResolvedDrawing(const Doll& doll, const QString& piece, int drawing) {
  assert(!piece.isEmpty());
  assert(drawing >= -1);
  const ArtPiece* art = FindArt(doll, piece);
  const RigPiece* rig = FindRig(doll.rig, piece);
  if (art == nullptr) {
    return -1;
  }
  const bool has_rig_default = rig != nullptr && rig->default_drawing >= 0;
  const int index = drawing >= 0     ? drawing
                    : has_rig_default ? rig->default_drawing
                                      : art->default_drawing;
  const bool is_in_range =
      index >= 0 && index < static_cast<int>(art->drawings.size());
  return is_in_range ? index : -1;
}

// The drawing index track's piece shows at frame, or -1.
int ShownDrawing(const Project& project, const TrackRef& track, Frame frame) {
  assert(track.kind == TrackKind::kPiece);
  assert(frame.index() >= 0);
  const Doll* doll = DollOfLayer(project, track.shot, track.layer);
  const DollLayer* layer = PosedLayerOf(project, track.shot, track.layer);
  if (doll == nullptr || layer == nullptr) {
    return -1;
  }
  const PoseMap poses = SamplePoses(*doll, *layer, frame);
  const auto pose = poses.find(track.piece);
  const int drawing = pose == poses.end() ? -1 : pose->second.drawing;
  return ResolvedDrawing(*doll, track.piece, drawing);
}

// drawing index of doll's piece on a clear Format_ARGB32 sheet of
// sheet's size whose frame starts at origin: the drawing's frame on the
// sheet's, what it draws past its frame around it.
QImage DrawingImage(const Doll& doll, const QString& piece, int drawing,
                    QSize sheet, QPoint origin) {
  assert(!piece.isEmpty());
  assert(sheet.isValid());
  QImage image(sheet, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  const QImage read(drawing >= 0 ? DrawingPath(doll, piece, drawing)
                                 : QString());
  if (!read.isNull()) {
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(origin - DrawingOffset(doll, piece, drawing), read);
  }
  return image;
}

// Room to draw past the frame on each side: half the frame's longer
// side, more when the drawing begun from already reaches further.
int SheetMargin(const Doll& doll, const QString& piece, int drawing,
                QSize frame) {
  assert(frame.isValid());
  assert(!piece.isEmpty());
  int margin = std::max(frame.width(), frame.height()) / 2;
  const QImage read(drawing >= 0 ? DrawingPath(doll, piece, drawing)
                                 : QString());
  const QPoint offset = DrawingOffset(doll, piece, drawing);
  const bool has_read = !read.isNull();
  if (has_read) {
    margin = std::max({margin, offset.x(), offset.y(),
                       read.width() - offset.x() - frame.width(),
                       read.height() - offset.y() - frame.height()});
  }
  return std::clamp(margin, 8, kMaxSheetMargin);
}

// "mouth 2", "mouth 3"...: the first name no drawing of art has.
QString FreeName(const ArtPiece& art) {
  assert(!art.name.isEmpty());
  assert(art.drawings.size() <= static_cast<size_t>(kMaxPieceDrawings));
  const int count = static_cast<int>(art.drawings.size());
  QStringList taken;
  for (int i = 0; i < count; ++i) {
    taken.append(DrawingName(art, i));
  }
  for (int number = count + 1; number <= 2 * kMaxPieceDrawings; ++number) {
    const QString name = QStringLiteral("%1 %2").arg(art.name).arg(number);
    if (!taken.contains(name)) {
      return name;
    }
  }
  return art.name + QStringLiteral(" new");
}

}  // namespace

DrawManager::DrawManager(HistoryManager* history, PlaybackManager* playback,
                         PaletteManager* palette)
    : history_(history), playback_(playback), palette_(palette) {
  assert(history_ != nullptr);
  assert(playback_ != nullptr);
  if (palette_ != nullptr) {
    connect(palette_, &PaletteManager::Picked, this,
            [this] { emit Changed(); });
  }
}

QString DrawManager::WhyNoDraw(const TrackRef& track, Frame frame,
                               bool is_edit) const {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  if (is_drawing_) {
    return Tr("Already drawing: press Done or Cancel first.");
  }
  const Project& project = history_->current();
  const bool is_piece = track.kind == TrackKind::kPiece;
  const Doll* doll =
      is_piece ? DollOfLayer(project, track.shot, track.layer) : nullptr;
  const ArtPiece* art = doll != nullptr ? FindArt(*doll, track.piece) : nullptr;
  if (art == nullptr) {
    return Tr("Pick a piece of a doll first.");
  }
  if (art->size.isEmpty() || doll->folder.isEmpty()) {
    return Tr("%1 has no drawing to take the size from.").arg(track.piece);
  }
  const int shown = is_edit ? ShownDrawing(project, track, frame) : -1;
  if (is_edit && shown < 0) {
    return Tr("%1 shows no drawing here to edit.").arg(track.piece);
  }
  const bool is_new_version =
      is_edit && OwnDrawingAt(*doll, track.piece, shown) != nullptr;
  const bool is_full =
      art->drawings.size() >= static_cast<size_t>(kMaxPieceDrawings);
  if (is_full && !is_new_version) {
    return Tr("A piece holds at most %1 drawings.").arg(kMaxPieceDrawings);
  }
  return QString();
}

Result<void> DrawManager::Begin(const TrackRef& track, Frame frame,
                                DrawStart start) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const QString why_not = WhyNoDraw(track, frame, false);
  if (!why_not.isEmpty()) {
    return std::unexpected(Error{why_not});
  }
  const Project& project = history_->current();
  const Doll& doll = *DollOfLayer(project, track.shot, track.layer);
  const int from = start == DrawStart::kShowing
                       ? ShownDrawing(project, track, frame)
                   : start == DrawStart::kDefault
                       ? ResolvedDrawing(doll, track.piece, -1)
                       : -1;
  return Start(track, frame, from, -1);
}

Result<void> DrawManager::BeginEdit(const TrackRef& track, Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const QString why_not = WhyNoDraw(track, frame, true);
  if (!why_not.isEmpty()) {
    return std::unexpected(Error{why_not});
  }
  const Project& project = history_->current();
  const Doll& doll = *DollOfLayer(project, track.shot, track.layer);
  const int shown = ShownDrawing(project, track, frame);
  const bool is_own = OwnDrawingAt(doll, track.piece, shown) != nullptr;
  return Start(track, frame, shown, is_own ? shown : -1);
}

Result<void> DrawManager::Start(const TrackRef& track, Frame frame, int from,
                                int replaces) {
  assert(!is_drawing_);
  assert(from >= -1 && replaces >= -1);
  const Project& project = history_->current();
  const Doll& doll = *DollOfLayer(project, track.shot, track.layer);
  const ArtPiece& art = *FindArt(doll, track.piece);
  if (playback_->IsPlaying()) {
    playback_->Pause();
  }
  target_ = DrawTarget::kPiece;
  under_ = QImage();
  keep_ = nullptr;
  doll_ = doll.name;
  track_ = track;
  frame_ = frame;
  replaces_ = replaces;
  const int shown = ShownDrawing(project, track, frame);
  // A sheet around the frame, to draw past it; Done keeps only what
  // was drawn, cropped, and where its frame starts.
  const int margin =
      SheetMargin(doll, track.piece, from >= 0 ? from : shown, art.size);
  frame_size_ = art.size;
  origin_ = QPoint(margin, margin);
  sheet_ = art.size + QSize(2 * margin, 2 * margin);
  image_ = DrawingImage(doll, track.piece, from, sheet_, origin_);
  reference_ = from >= 0
                   ? image_
                   : DrawingImage(doll, track.piece, shown, sheet_, origin_);
  doll_drawings_.clear();
  const DollLayer* posed = PosedLayerOf(project, track.shot, track.layer);
  if (posed != nullptr) {
    for (const auto& [name, pose] : SamplePoses(doll, *posed, frame)) {
      doll_drawings_[name] = pose.drawing;
    }
  }
  const auto places = PieceTransforms(doll, PoseMap());
  const auto place = places.find(track.piece);
  // Sheet pixels to doll space: the frame's pixels sit origin_ in.
  rest_place_ = QTransform::fromTranslate(-origin_.x(), -origin_.y()) *
                (place != places.end() ? place->second : QTransform());
  const bool is_replacing = replaces >= 0;
  default_name_ = is_replacing ? DrawingName(art, replaces) : FreeName(art);
  const bool is_krita_copy = from >= 0 && !is_replacing &&
                             OwnDrawingAt(doll, track.piece, from) == nullptr;
  title_ = is_krita_copy
               ? Tr("Drawing: copy of %1 (%2)")
                     .arg(DrawingName(art, from), track.piece)
               : Tr("Drawing: %1 (%2)").arg(default_name_, track.piece);
  flipped_ = -1;
  StackPages(static_cast<int>(art.drawings.size()), from >= 0 ? from : shown,
             replaces);
  // The onion is the pages either side of the new one in the stack, as
  // an animator lays the drawings before and after under an in-between;
  // with none, the drawing it started from.
  onions_.clear();
  for (const int side : {page_ - 1, page_ + 1}) {
    const bool is_page =
        side >= 0 && side < static_cast<int>(pages_.size()) &&
        pages_[static_cast<size_t>(side)] >= 0;
    if (is_page) {
      onions_.push_back(DrawingImage(doll, track.piece,
                                     pages_[static_cast<size_t>(side)],
                                     sheet_, origin_));
    }
  }
  if (onions_.empty()) {
    onions_ = {reference_};
  }
  Open();
  return {};
}

void DrawManager::Open() {
  assert(!is_drawing_);
  assert(!image_.isNull());
  undo_.clear();
  redo_.clear();
  is_stroking_ = false;
  has_last_ = false;
  is_peeking_ = false;
  is_drawing_ = true;
  did_open_palette_ = palette_ != nullptr && !palette_->is_shown();
  const bool has_palette = palette_ != nullptr;
  if (has_palette) {
    palette_->SetUse(PaletteUse::kDoll);
  }
  if (did_open_palette_) {
    palette_->SetShown(true);
  }
  emit Started();
  emit Changed();
}

QString DrawManager::Title() const {
  assert(is_drawing_ || title_.size() >= 0);
  assert(title_.size() < 100000);
  return title_;
}

void DrawManager::SetTool(DrawTool tool) {
  assert(static_cast<int>(tool) >= 0);
  assert(static_cast<int>(tool) < kDrawToolCount);
  tool_ = tool;
  emit Changed();
}

void DrawManager::SetBrushSize(int size) {
  assert(size > -1000000 && size < 1000000);
  brush_size_ = std::clamp(size, 1, kMaxBrushSize);
  assert(brush_size_ >= 1);
  emit Changed();
}

QColor DrawManager::color() const {
  const bool has_palette = palette_ != nullptr && palette_->current().isValid();
  assert(own_color_.isValid());
  return has_palette ? palette_->current() : own_color_;
}

void DrawManager::SetPeeking(bool is_peeking) {
  assert(is_drawing_ || !is_peeking);
  is_peeking_ = is_peeking;
  emit Changed();
}

void DrawManager::SetOnion(bool is_onion) {
  assert(is_drawing_ || !is_onion);
  is_onion_ = is_onion;
  emit Changed();
}

void DrawManager::StackPages(int count, int after, int replaces) {
  assert(count >= 0);
  assert(replaces < count);
  pages_.clear();
  const bool is_after_none = after < 0 || after >= count;
  if (is_after_none) {
    pages_.push_back(-1);
  }
  for (int i = 0; i < count; ++i) {
    // An edit takes the place of the drawing it replaces.
    const bool is_replaced = i == replaces;
    pages_.push_back(is_replaced ? -1 : i);
    const bool goes_after = i == after && replaces < 0;
    if (goes_after) {
      pages_.push_back(-1);
    }
  }
  page_ = static_cast<int>(
      std::find(pages_.begin(), pages_.end(), -1) - pages_.begin());
  assert(page_ < static_cast<int>(pages_.size()));
}

void DrawManager::Flip(int step) {
  assert(history_ != nullptr);
  assert(step > -1000 && step < 1000);
  const bool can_flip = is_drawing_ && target_ == DrawTarget::kPiece;
  if (!can_flip) {
    return;
  }
  const Project& project = history_->current();
  const Doll* doll = DollOfLayer(project, track_.shot, track_.layer);
  const ArtPiece* art =
      doll != nullptr ? FindArt(*doll, track_.piece) : nullptr;
  const int count =
      art != nullptr ? static_cast<int>(art->drawings.size()) : 0;
  if (count == 0) {
    return;
  }
  // A drawing added since draw mode began is not in the stack: the
  // stack keeps to what was there.
  const int pages = static_cast<int>(pages_.size());
  if (pages <= 1) {
    return;
  }
  page_ = ((page_ + step) % pages + pages) % pages;
  flipped_ = std::min(pages_[static_cast<size_t>(page_)], count - 1);
  const bool is_other = flipped_ >= 0;
  flipped_image_ = is_other
                       ? DrawingImage(*doll, track_.piece, flipped_, sheet_,
                                      origin_)
                       : QImage();
  flipped_name_ = is_other ? DrawingName(*art, flipped_) : QString();
  emit Changed();
}

void DrawManager::Press(QPoint at, bool is_erase, bool is_line) {
  assert(is_drawing_);
  assert(!image_.isNull());
  // Drawing lands on the new drawing: flip back to it first.
  if (is_flipped()) {
    flipped_ = -1;
    page_ = static_cast<int>(
        std::find(pages_.begin(), pages_.end(), -1) - pages_.begin());
    flipped_image_ = QImage();
    flipped_name_.clear();
    emit Changed();
  }
  const bool is_picker = tool_ == DrawTool::kPicker && !is_erase;
  if (is_picker) {
    PickColor(at);
    return;
  }
  const bool is_move = tool_ == DrawTool::kMove;
  if (is_move) {
    Remember(image_);
    move_base_ = image_;
    move_from_ = at;
    is_moving_ = true;
    is_stroking_ = true;
    return;
  }
  const bool is_fill = tool_ == DrawTool::kFill && !is_erase;
  if (is_fill) {
    const QImage before = image_;
    if (FloodFill(&image_, at, color())) {
      Remember(before);
      emit Changed();
    }
    return;
  }
  Remember(image_);
  const bool is_clearing = is_erase || tool_ == DrawTool::kEraser;
  stroke_color_ = is_clearing ? QColor(Qt::transparent) : color();
  if (is_line && has_last_) {
    PaintLine(&image_, last_, at, brush_size_, stroke_color_);
  } else {
    PaintDab(&image_, at, brush_size_, stroke_color_);
  }
  last_ = at;
  has_last_ = true;
  is_stroking_ = true;
  emit Changed();
}

void DrawManager::MoveTo(QPoint at) {
  assert(is_drawing_ || !is_stroking_);
  assert(brush_size_ >= 1);
  if (is_moving_) {
    // From the drawing as pressed, so dragging back brings back what
    // went past the frame; it is cut off only when let go.
    QImage shifted(move_base_.size(), QImage::Format_ARGB32);
    shifted.fill(Qt::transparent);
    QPainter painter(&shifted);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(at - move_from_, move_base_);
    painter.end();
    image_ = shifted;
    emit Changed();
    return;
  }
  if (!is_stroking_ || at == last_) {
    return;
  }
  PaintLine(&image_, last_, at, brush_size_, stroke_color_);
  last_ = at;
  emit Changed();
}

void DrawManager::Release() {
  assert(is_drawing_ || !is_stroking_);
  assert(undo_.size() <= static_cast<size_t>(kMaxDrawUndo));
  // A move that went nowhere leaves nothing to undo.
  const bool was_still =
      is_moving_ && !undo_.empty() && undo_.back() == image_;
  if (was_still) {
    undo_.pop_back();
  }
  is_moving_ = false;
  move_base_ = QImage();
  is_stroking_ = false;
}

void DrawManager::PickColor(QPoint at) {
  assert(is_drawing_);
  assert(!image_.isNull());
  const bool is_inside = image_.rect().contains(at);
  QColor picked = is_inside ? image_.pixelColor(at) : QColor();
  const bool is_solid = picked.isValid() && picked.alpha() > 0;
  if (!is_solid) {
    return;
  }
  picked.setAlpha(255);
  if (palette_ != nullptr) {
    palette_->Pick(picked);
  } else {
    own_color_ = picked;
    emit Changed();
  }
}

void DrawManager::Undo() {
  assert(undo_.size() <= static_cast<size_t>(kMaxDrawUndo));
  assert(!is_stroking_ || is_drawing_);
  if (undo_.empty() || is_stroking_) {
    return;
  }
  redo_.push_back(image_);
  image_ = undo_.back();
  undo_.pop_back();
  emit Changed();
}

void DrawManager::Redo() {
  assert(redo_.size() <= static_cast<size_t>(kMaxDrawUndo) + 1);
  assert(!is_stroking_ || is_drawing_);
  if (redo_.empty() || is_stroking_) {
    return;
  }
  undo_.push_back(image_);
  image_ = redo_.back();
  redo_.pop_back();
  emit Changed();
}

void DrawManager::Remember(const QImage& before) {
  assert(is_drawing_);
  assert(!before.isNull());
  undo_.push_back(before);
  const bool is_too_long = undo_.size() > static_cast<size_t>(kMaxDrawUndo);
  if (is_too_long) {
    undo_.erase(undo_.begin());
  }
  redo_.clear();
}

void DrawManager::Cancel() {
  assert(history_ != nullptr);
  assert(undo_.size() <= static_cast<size_t>(kMaxDrawUndo));
  if (is_drawing_) {
    End();
  }
}

void DrawManager::End() {
  assert(is_drawing_);
  assert(history_ != nullptr);
  is_drawing_ = false;
  is_stroking_ = false;
  is_peeking_ = false;
  image_ = QImage();
  reference_ = QImage();
  under_ = QImage();
  onions_.clear();
  keep_ = nullptr;
  undo_.clear();
  redo_.clear();
  const bool has_palette = palette_ != nullptr;
  if (has_palette) {
    palette_->SetUse(PaletteUse::kAuto);
  }
  if (did_open_palette_ && palette_ != nullptr) {
    palette_->SetShown(false);
  }
  did_open_palette_ = false;
  emit Ended();
  emit Changed();
}

}  // namespace snapper
