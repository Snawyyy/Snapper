#ifndef SNAPPER_ANIM_MASK_RINGS_H_
#define SNAPPER_ANIM_MASK_RINGS_H_

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRect>

#include <cstdint>
#include <vector>

namespace snapper {

// Which pixels of a piece's drawings are visible: a pixel is where any
// drawing is at least threshold opaque (0 to 255), grown by grow pixels
// every way (so it reaches grow pixels past the drawings' edges too).
// Coordinates are the drawings' own pixels.
class AlphaMask final {
 public:
  AlphaMask(const std::vector<QImage>& drawings, int threshold, int grow);

  // The pixels the mask covers: the drawings and grow all round.
  QRect area() const { return area_; }
  bool IsVisible(QPoint at) const;
  // Whether nothing at all is visible.
  bool IsEmpty() const { return is_empty_; }

 private:
  size_t Index(QPoint at) const;
  void Grow(int grow);

  QRect area_;
  std::vector<std::uint8_t> bits_;
  bool is_empty_ = true;
};

// One outline followed round a mask: the corners where it turns, and
// the corner at the start of every step it took heading right (the top
// sides of the visible pixels below them), so a caller can tell which
// outlines it has been round.
struct FollowedRing final {
  std::vector<QPointF> turns;
  std::vector<QPoint> rightward;
};

// Follows the outline of the shape from corner start (the top-left
// corner of a visible pixel with nothing above it), heading right, with
// the shape on the right, until it is back.
FollowedRing FollowEdges(const AlphaMask& mask, QPoint start);

// Every outline of mask, the outside of each shape and each hole in
// one, as closed rings of the corners where they turn, each with the
// shape on its right (so outsides run clockwise on a screen, holes the
// other way). At most max_rings, the first met from the top left first.
std::vector<std::vector<QPointF>> AllRings(const AlphaMask& mask,
                                           int max_rings);

}  // namespace snapper

#endif  // SNAPPER_ANIM_MASK_RINGS_H_
