#ifndef SNAPPER_UI_TRANSITION_PICKER_H_
#define SNAPPER_UI_TRANSITION_PICKER_H_

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QWidget>

#include "base/frame.h"
#include "model/transition.h"

namespace snapper {

// Picks how one clip hands over to the next on the reel: the kind
// (a cut, a swipe, a flash or a crossfade) and, for anything but a
// cut, how many frames it runs, at most longest. The answer is
// transition().
class TransitionPicker final : public QDialog {
  Q_OBJECT

 public:
  TransitionPicker(const Transition& now, Frame longest,
                   QWidget* parent = nullptr);

  Transition transition() const;

 private:
  void Show();

  Frame longest_;
  QFormLayout layout_;
  QComboBox kind_;
  QSpinBox length_;
  QDialogButtonBox buttons_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TRANSITION_PICKER_H_
