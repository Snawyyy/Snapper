#ifndef SNAPPER_EDIT_DIAL_MANAGER_H_
#define SNAPPER_EDIT_DIAL_MANAGER_H_

#include <QString>

#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "model/project.h"
#include "model/smart_dial.h"

namespace snapper {

class HistoryManager;

// What a new dial starts from: a template only names the stops (and
// picks the swap), the art is the user's.
enum class DialTemplate { kHeadTurn, kBlink, kMouth, kBlank };
inline constexpr int kDialTemplateCount = 4;

// The name a template tile shows, and a new dial is called.
QString TemplateName(DialTemplate kind);

// Smart dials: making them on a doll, setting their stops from a posed
// layer, and keying their value on doll layers. A dial lives in the
// doll's rig, so it comes with the doll into every shot; its value is
// keyed per layer like the rest of the pose. Every call is one undo
// step, or part of a drag.
class DialManager final {
 public:
  explicit DialManager(HistoryManager* history);

  // A dial on doll from kind, driving pieces, named after the template
  // ("Blink", "Blink 2"...). Returns the name.
  Result<QString> AddDial(const QString& doll, DialTemplate kind,
                          const std::vector<QString>& pieces);
  // Removes the dial and its keys on every layer of the doll.
  Result<void> RemoveDial(const QString& doll, const QString& dial);
  // Renames it, its keys on every layer of the doll too.
  Result<void> RenameDial(const QString& doll, const QString& dial,
                          const QString& to);
  // The pieces it drives.
  Result<void> SetPieces(const QString& doll, const QString& dial,
                         const std::vector<QString>& pieces);
  // Moves the dial up (-1) or down (+1) the list; for the drawing, the
  // one lower in the list wins.
  Result<void> MoveDial(const QString& doll, const QString& dial, int by);

  // Saves how layer shows the dial's pieces at frame as the stop at
  // value: the stop already within snapping distance of value (keeping
  // its name), or a new one called name. The hand pose of those pieces
  // at frame goes into the stop and their hand keys there go back to
  // rest, and the layer's dial is keyed at the stop, so the doll looks
  // the same after. One step.
  Result<void> SetStop(ShotId shot, LayerId layer, Frame frame,
                       const QString& dial, double value,
                       const QString& name);
  Result<void> RemoveStop(const QString& doll, const QString& dial,
                          int stop);
  Result<void> RenameStop(const QString& doll, const QString& dial,
                          int stop, const QString& to);
  // Moves a stop to a new place on the dial (kept apart from the
  // others).
  Result<void> MoveStop(const QString& doll, const QString& dial, int stop,
                        double at);
  Result<void> SetRest(const QString& doll, const QString& dial, int stop);
  Result<void> SetSwap(const QString& doll, const QString& dial,
                       DialSwap swap);

  // Keys the dial called dial at value on each of layers at frame;
  // layers whose doll has no such dial are left alone, so a crowd of
  // dolls with the same dial turns together. A drag calls it on every
  // move inside an EditScope.
  Result<void> Turn(ShotId shot, const std::vector<LayerId>& layers,
                    Frame frame, const QString& dial, double value);

  // The dials shown for layer: all of its doll's, or with piece given,
  // only those that drive it.
  std::vector<SmartDial> DialsOf(ShotId shot, LayerId layer,
                                 const QString& piece) const;
  // The value layer has for dial at frame.
  double ValueOf(ShotId shot, LayerId layer, Frame frame,
                 const QString& dial) const;
  // Why a dial can't be made from pieces on layer; empty when it can.
  QString WhyNoDial(ShotId shot, LayerId layer,
                    const std::vector<QString>& pieces) const;

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_DIAL_MANAGER_H_
