#ifndef SNAPPER_MODEL_SMART_DIAL_H_
#define SNAPPER_MODEL_SMART_DIAL_H_

#include <QString>

#include <map>
#include <vector>

#include "model/pose.h"

namespace snapper {

constexpr int kMaxSmartDials = 32;
constexpr int kMaxDialStops = 8;
constexpr int kMaxDialPieces = 64;

// One saved place on a smart dial: where it sits on the dial and how
// it poses each piece the dial drives. A pose is added under the hand
// pose (turn, move, skew and order add, scale and opacity multiply);
// its drawing and warp are not used. The drawing is named by its Krita
// layer name (see DrawingName), so a re-export in a new order keeps it.
struct DialStop final {
  QString name;
  // 0 (the dial's left end) to 1 (its right end).
  double at = 0.0;
  std::map<QString, PiecePose> poses;
  // Piece to drawing name; a piece missing here keeps its drawing.
  std::map<QString, QString> drawings;

  bool operator==(const DialStop&) const = default;
};

// Where between two stops the drawings swap: halfway (good for turns)
// or only on reaching the next stop (good for blinks).
enum class DialSwap { kHalfway, kAtStop };

// A named control on a doll that moves several pieces at once between
// saved stops: a head turn, a blink, a mouth. Its value is keyed on
// each doll layer like the rest of the pose.
struct SmartDial final {
  QString name;
  // The pieces it drives.
  std::vector<QString> pieces;
  // Sorted by at, never two at the same place.
  std::vector<DialStop> stops;
  // The stop the dial sits on when not keyed, and goes back to on a
  // double click.
  int rest = 0;
  DialSwap swap = DialSwap::kHalfway;

  bool operator==(const SmartDial&) const = default;
};

// The dial called name, or nullptr.
const SmartDial* FindDial(const std::vector<SmartDial>& dials,
                          const QString& name);
SmartDial* FindDial(std::vector<SmartDial>* dials, const QString& name);

// Where dial sits when not keyed: its rest stop, or the middle.
double RestValue(const SmartDial& dial);

}  // namespace snapper

#endif  // SNAPPER_MODEL_SMART_DIAL_H_
