#ifndef SNAPPER_UI_DISC_BURNER_H_
#define SNAPPER_UI_DISC_BURNER_H_

#include <QString>
#include <QTimer>
#include <QWidget>

namespace snapper {

// The export's progress as a DVD burner: a blank disc sits in a dark
// tray, spins while frames are written, a red bar and a counter under it
// fill up, and when it is done the disc wears its file's name, written
// on it in marker. It shows what ExportManager reports and decides
// nothing.
class DiscBurner final : public QWidget {
  Q_OBJECT

 public:
  enum class State { kEmpty, kBurning, kDone, kFailed };

  explicit DiscBurner(QWidget* parent = nullptr);

  State state() const { return state_; }
  // done of all frames are written; starts the spin.
  void SetProgress(int done, int all);
  // The disc is burned: it stops and wears label.
  void Finish(const QString& label);
  // Nothing was written: the tray goes back to empty, or shows a failed
  // disc.
  void Stop(bool has_failed);

  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  State state_ = State::kEmpty;
  int done_ = 0;
  int all_ = 0;
  QString label_;
  // Degrees the disc has turned.
  double turn_ = 0.0;
  QTimer spin_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DISC_BURNER_H_
