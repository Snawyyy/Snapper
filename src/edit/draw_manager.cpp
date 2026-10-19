#include "edit/draw_manager.h"

#include <QPainter>

#include <algorithm>
#include <cassert>

#include "base/text.h"
#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "edit/playback_manager.h"
#include "render/pixel_paint.h"

namespace snapper {

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
    palette_->SetUse(PaletteUse::kCharacter);
  }
  if (did_open_palette_) {
    palette_->SetShown(true);
  }
  emit Started();
  emit Changed();
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

void DrawManager::Press(QPoint at, bool is_erase, bool is_line) {
  assert(is_drawing_);
  assert(!image_.isNull());
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

Result<void> DrawManager::Done() {
  assert(history_ != nullptr);
  assert(keep_ != nullptr || !is_drawing_);
  if (!is_drawing_) {
    return std::unexpected(Error{Tr("Nothing is being drawn.")});
  }
  auto kept = keep_(image_);
  if (kept) {
    End();
  }
  return kept;
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
