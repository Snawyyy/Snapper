#include "ui/slot_picker.h"

#include <QFileDialog>
#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <variant>

#include "edit/history_manager.h"
#include "edit/reel_manager.h"
#include "ui/form_helpers.h"

namespace snapper {
namespace {

// How wide the picture of the gap's first frame is drawn.
constexpr int kPictureWidth = 360;

}  // namespace

SlotPicker::SlotPicker(const Managers& managers, Slot slot,
                       const SlotFill& fill, QWidget* parent)
    : QDialog(parent),
      managers_(managers),
      slot_(slot),
      fill_(fill),
      layout_(this),
      start_(Qt::Horizontal),
      other_(tr("Other video...")),
      buttons_(QDialogButtonBox::Ok | QDialogButtonBox::Cancel) {
  assert(managers_.IsComplete());
  assert(slot_.IsValid());
  setWindowTitle(tr("Fill the gap"));
  picture_.setMinimumSize(kPictureWidth, kPictureWidth * 9 / 16);
  picture_.setAlignment(Qt::AlignCenter);
  start_.setObjectName("slot_start");
  start_.setToolTip(tr("Slides the video under the gap: which part of "
                       "it plays there."));
  layout_.addWidget(&picture_);
  layout_.addWidget(&start_);
  layout_.addWidget(&where_);
  layout_.addWidget(&other_);
  layout_.addWidget(&buttons_);
  connect(&start_, &QSlider::valueChanged, this, &SlotPicker::SetIn);
  connect(&other_, &QPushButton::clicked, this, &SlotPicker::AskForVideo);
  connect(&buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(&buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
  Show();
}

void SlotPicker::UseVideo(const QString& path) {
  assert(managers_.reel != nullptr);
  assert(slot_.IsValid());
  const auto video = managers_.reel->ReadVideo(path);
  if (!video) {
    where_.setText(video.error().message);
    return;
  }
  fill_ = SlotFill{*video, Frame(0)};
  Show();
}

void SlotPicker::AskForVideo() {
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Fill the gap from"), QString(),
      tr("Videos (*.mp4 *.mov *.mkv *.webm *.avi *.m4v);;All files (*)"));
  const bool is_picked = !path.isEmpty();
  if (is_picked) {
    UseVideo(path);
  }
}

void SlotPicker::SetIn(int in) {
  assert(in >= 0);
  assert(slot_.IsValid());
  fill_.in = Frame(in);
  Show();
}

void SlotPicker::Show() {
  assert(managers_.history != nullptr);
  assert(slot_.IsValid());
  const int last =
      LastSlotIn(managers_.history->current(), fill_.source, slot_);
  const bool fits = last >= 0;
  const QString short_video =
      fits ? QString() : tr("That video is shorter than the gap.");
  Explain(&start_, short_video);
  Explain(buttons_.button(QDialogButtonBox::Ok), short_video);
  fill_.in = Frame(std::clamp(fill_.in.index(), 0, std::max(last, 0)));
  {
    const QSignalBlocker quiet(start_);
    start_.setRange(0, std::max(last, 0));
    start_.setValue(fill_.in.index());
  }
  const Frame end(fill_.in.index() + slot_.length().index());
  const Frame total(last + slot_.length().index());
  where_.setText(fits ? tr("Plays %1 to %2 of %3")
                            .arg(TimeText(fill_.in), TimeText(end),
                                 TimeText(total))
                      : short_video);
  const auto* video = std::get_if<VideoSource>(&fill_.source);
  const bool is_video = video != nullptr;
  const QImage first =
      is_video ? videos_.Picture(video->path, fill_.in) : QImage();
  const bool has_picture = !first.isNull();
  if (has_picture) {
    picture_.setPixmap(QPixmap::fromImage(
        first.scaled(picture_.minimumSize(), Qt::KeepAspectRatio)));
  } else {
    picture_.setText(is_video ? tr("No picture") : tr("A shot"));
  }
}

}  // namespace snapper
