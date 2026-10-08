#ifndef SNAPPER_UI_SHOT_BIN_H_
#define SNAPPER_UI_SHOT_BIN_H_

#include <QIcon>
#include <QListWidget>
#include <QMimeData>
#include <QStringList>

#include <map>
#include <memory>

#include "model/shot.h"
#include "render/frame_renderer.h"
#include "ui/managers.h"

namespace snapper {

// What a shot dragged out of the shot list carries: its id, as text.
inline constexpr char kShotMime[] = "application/x-snapper-shot";

// The shot for a drop, or an invalid id when it carries none.
ShotId ShotOfDrop(const QMimeData* data);

// Every shot, with a picture of its first frame and its length, ready to
// drag onto the reel as a finished video.
class ShotBin final : public QListWidget {
  Q_OBJECT

 public:
  explicit ShotBin(const Managers& managers);
  // Lists the shots as they are now.
  void Refresh();

 protected:
  void showEvent(QShowEvent* event) override;
  QStringList mimeTypes() const override;
  QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override;

 private:
  QIcon Thumbnail(const std::shared_ptr<const Shot>& shot);

  Managers managers_;
  FrameRenderer renderer_;
  // Pictures by the shot value they were drawn from; an edited shot is
  // a new value, so it gets a new picture.
  std::map<std::shared_ptr<const Shot>, QIcon> thumbnails_;
  // Edits made while the list was hidden, so it lists afresh on show.
  bool is_stale_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_BIN_H_
