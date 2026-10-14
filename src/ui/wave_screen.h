#ifndef SNAPPER_UI_WAVE_SCREEN_H_
#define SNAPPER_UI_WAVE_SCREEN_H_

#include <QString>
#include <QTimer>
#include <QWidget>

#include <vector>

class QMouseEvent;
class QPaintEvent;

namespace snapper {

class AudioPreview;

// A sound file drawn as a waveform on a little green phosphor screen,
// sunk into the panel like an old scope: a grid, the wave, the length as
// a readout in the corner, a name in the other. While preview plays the
// file a red line runs along it; a click plays from where it lands.
// Without a file it shows idle text ("NO SIGNAL").
class WaveScreen final : public QWidget {
  Q_OBJECT

 public:
  WaveScreen(AudioPreview* preview, const QString& idle, QWidget* parent);

  // Shows path (decoded once here), named name; an empty path clears it.
  void SetFile(const QString& path, const QString& name);
  const QString& path() const { return path_; }
  double seconds() const { return seconds_; }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;

 private:
  void FollowPreview();
  void PaintGrid(QPainter* painter, const QRectF& glass) const;
  void PaintWave(QPainter* painter, const QRectF& glass) const;
  void PaintLabels(QPainter* painter, const QRectF& glass) const;

  AudioPreview* preview_;
  QString idle_;
  QString path_;
  QString name_;
  double seconds_ = 0.0;
  // The loudest sample of each column, 0 to 1.
  std::vector<float> peaks_;
  // Repaints the running line while the file plays.
  QTimer tick_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_WAVE_SCREEN_H_
