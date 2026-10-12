#ifndef SNAPPER_EDIT_OBJECT_MANAGER_H_
#define SNAPPER_EDIT_OBJECT_MANAGER_H_

#include <QDateTime>
#include <QPointF>
#include <QString>
#include <QStringList>

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "model/layer.h"
#include "model/shot.h"

namespace snapper {

class DollLibraryManager;
class HistoryManager;
class LinkManager;
class StageManager;

// Objects: props such as a cup or a mic made from a PNG or GIF. Each is
// a doll with one piece whose drawings are the picture's frames (see
// ObjectClip), kept in the doll library like any doll, so warp, jelly,
// skins and drawing on a piece work on it unchanged. This manager makes
// them from picture files, puts them on the stage, sets how a GIF plays
// (ObjectPlay) and has doll pieces hold and drop them (Hold).
class ObjectManager final {
 public:
  ObjectManager(HistoryManager* history, DollLibraryManager* library,
                StageManager* stage, LinkManager* links);

  // Library side. True when the library doll called name is an object.
  bool IsObject(const QString& name) const;
  // The library's objects, and its dolls that are not objects, sorted.
  QStringList Objects() const;
  QStringList Dolls() const;

  // Makes a library object of every picture paths name (a folder gives
  // each picture in it), named after its file; their names. The library
  // is files, so this is no undo step. notes, when given, gets a line
  // for each picture that had too many frames and was thinned.
  Result<QStringList> AddToLibrary(const QStringList& paths,
                                   QStringList* notes = nullptr);
  // Makes objects of the pictures and puts them on shot as one undo
  // step ("Add cup"), each centred on at (shot space; the middle when
  // none), at its own pixel size. The new layers.
  Result<std::vector<LayerId>> AddToShot(ShotId shot,
                                         const QStringList& paths,
                                         std::optional<QPointF> at,
                                         QStringList* notes = nullptr);

  // True when layer of shot shows an object.
  bool IsObjectLayer(ShotId shot, LayerId layer) const;
  // The object layers among layers.
  std::vector<LayerId> ObjectLayers(ShotId shot,
                                    const std::vector<LayerId>& layers) const;

  // Why objects can't be held by holder from a frame on, for a greyed
  // out menu entry; empty when they can.
  QString WhyNoHold(ShotId shot, const std::vector<LayerId>& objects,
                    const LinkEnd& holder) const;
  // From frame at on, holder holds objects: they follow its place, turn
  // and scale without jumping. One undo step, "Hold cup".
  Result<void> Hold(ShotId shot, const std::vector<LayerId>& objects,
                    const LinkEnd& holder, Frame at);
  QString WhyNoDrop(ShotId shot, const std::vector<LayerId>& objects,
                    Frame at) const;
  // Lets go of objects at frame at; they stay where they are then.
  Result<void> Drop(ShotId shot, const std::vector<LayerId>& objects,
                    Frame at);
  QString WhyNoUndoChange(ShotId shot, const std::vector<LayerId>& objects,
                          Frame at) const;
  // Takes away the hold changes of objects on frame at, so whatever held
  // them before goes on holding.
  Result<void> RemoveChange(ShotId shot, const std::vector<LayerId>& objects,
                            Frame at);
  // What holds object at frame, or nothing when it is free.
  std::optional<LinkEnd> HolderAt(ShotId shot, LayerId object,
                                  Frame at) const;
  // Each hold change of object, earliest first, as "12: held by Mia's
  // hand" or "40: dropped".
  QStringList HoldTrack(ShotId shot, LayerId object) const;

  // Why the frames per drawing or the other play settings can't be set.
  QString WhyNoPlay(ShotId shot, const std::vector<LayerId>& objects) const;
  // Sets how objects play their frames, as one undo step.
  Result<void> SetPlay(ShotId shot, const std::vector<LayerId>& objects,
                       const ObjectPlay& play);
  // The play settings of object, or the defaults when it is none.
  ObjectPlay PlayOf(ShotId shot, LayerId object) const;
  // How many frames the object's picture has.
  int FrameCount(ShotId shot, LayerId object) const;

 private:
  // The names of the objects, joined for undo labels.
  QString NamesOf(ShotId shot, const std::vector<LayerId>& objects) const;
  // True when holder moves with object: it is held by it, directly or
  // through other holds at any frame.
  bool WouldLoop(const Shot& shot, LayerId object,
                 const LinkEnd& holder) const;

  HistoryManager* history_;
  DollLibraryManager* library_;
  StageManager* stage_;
  LinkManager* links_;
  // Whether each library doll is an object, with the time its rig.json
  // was written when that was read.
  mutable std::map<QString, std::pair<QDateTime, bool>> kinds_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_OBJECT_MANAGER_H_
