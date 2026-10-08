#ifndef SNAPPER_UI_SLOT_PICKER_H_
#define SNAPPER_UI_SLOT_PICKER_H_

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QVBoxLayout>

#include "anim/reel_timeline.h"
#include "edit/video_file_frames.h"
#include "ui/managers.h"

namespace snapper {

// Picks which part of a video fills the gap between two cut markers:
// the gap's length is fixed, and the slider slides the video under it,
// with the picture it would start on shown above. Other video swaps the
// file. The answer is source() from in().
class SlotPicker final : public QDialog {
  Q_OBJECT

 public:
  SlotPicker(const Managers& managers, Slot slot, const SlotFill& fill,
             QWidget* parent = nullptr);

  const ClipSource& source() const { return fill_.source; }
  Frame in() const { return fill_.in; }
  // Swaps the video to the file at path; the file dialog's answer.
  void UseVideo(const QString& path);

 private:
  void AskForVideo();
  void SetIn(int in);
  void Show();

  Managers managers_;
  Slot slot_;
  SlotFill fill_;
  VideoFileFrames videos_;
  QVBoxLayout layout_;
  QLabel picture_;
  QSlider start_;
  QLabel where_;
  QPushButton other_;
  QDialogButtonBox buttons_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SLOT_PICKER_H_
