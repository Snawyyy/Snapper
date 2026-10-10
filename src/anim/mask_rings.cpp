#include "anim/mask_rings.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>

namespace snapper {
namespace {

// The widest drawing a mask is made for, so its loops stay bounded.
constexpr int kMaxMaskSide = 1 << 14;

// Heading 0 east, 1 south, 2 west, 3 north (y down).
constexpr std::array<QPoint, 4> kStep = {
    QPoint(1, 0), QPoint(0, 1), QPoint(-1, 0), QPoint(0, -1)};
// The pixels ahead of a vertex to the left and right of each heading.
constexpr std::array<QPoint, 4> kAheadLeft = {
    QPoint(0, -1), QPoint(0, 0), QPoint(-1, 0), QPoint(-1, -1)};
constexpr std::array<QPoint, 4> kAheadRight = {
    QPoint(0, 0), QPoint(-1, 0), QPoint(-1, -1), QPoint(0, -1)};

// Sets each of count cells (step apart from first) when a set cell is
// within grow of it along the line.
void GrowLine(std::uint8_t* first, int count, int step, int grow,
              std::vector<int>* reach) {
  assert(first != nullptr && reach != nullptr);
  assert(count >= 0 && step > 0 && grow >= 0);
  reach->assign(static_cast<size_t>(count), kMaxMaskSide);
  int last = -kMaxMaskSide;
  for (int i = 0; i < count; ++i) {
    const bool is_set = first[static_cast<size_t>(i * step)] != 0;
    last = is_set ? i : last;
    (*reach)[static_cast<size_t>(i)] = i - last;
  }
  last = 2 * kMaxMaskSide;
  for (int i = count - 1; i >= 0; --i) {
    const bool is_set = first[static_cast<size_t>(i * step)] != 0;
    last = is_set ? i : last;
    const int nearest = std::min((*reach)[static_cast<size_t>(i)], last - i);
    first[static_cast<size_t>(i * step)] = nearest <= grow ? 1 : 0;
  }
}

}  // namespace

AlphaMask::AlphaMask(const std::vector<QImage>& drawings, int threshold,
                     int grow) {
  assert(threshold >= 0 && threshold <= 255);
  assert(grow >= 0 && grow < kMaxMaskSide);
  QSize size(0, 0);
  for (const QImage& drawing : drawings) {
    size = size.expandedTo(drawing.size());
  }
  size = size.boundedTo(QSize(kMaxMaskSide, kMaxMaskSide));
  area_ = QRect(-grow, -grow, size.width() + 2 * grow,
                size.height() + 2 * grow);
  bits_.assign(static_cast<size_t>(area_.width()) *
                   static_cast<size_t>(area_.height()),
               0);
  for (const QImage& drawing : drawings) {
    const QImage plain = drawing.convertToFormat(QImage::Format_ARGB32);
    const int width = std::min(plain.width(), size.width());
    const int height = std::min(plain.height(), size.height());
    for (int y = 0; y < height; ++y) {
      const auto* row = reinterpret_cast<const QRgb*>(plain.constScanLine(y));
      for (int x = 0; x < width; ++x) {
        const bool is_seen = qAlpha(row[x]) >= threshold;
        if (is_seen) {
          bits_[Index(QPoint(x, y))] = 1;
          is_empty_ = false;
        }
      }
    }
  }
  Grow(grow);
}

bool AlphaMask::IsVisible(QPoint at) const {
  assert(bits_.size() == static_cast<size_t>(area_.width()) *
                             static_cast<size_t>(area_.height()));
  assert(area_.width() >= 0);
  const bool is_inside = area_.contains(at);
  return is_inside && bits_[Index(at)] != 0;
}

size_t AlphaMask::Index(QPoint at) const {
  assert(area_.contains(at));
  assert(!bits_.empty());
  return static_cast<size_t>(at.y() - area_.top()) *
             static_cast<size_t>(area_.width()) +
         static_cast<size_t>(at.x() - area_.left());
}

void AlphaMask::Grow(int grow) {
  assert(grow >= 0);
  assert(area_.width() >= 0 && area_.height() >= 0);
  const bool is_growing = grow > 0 && !is_empty_;
  if (!is_growing) {
    return;
  }
  std::vector<int> reach;
  const int width = area_.width();
  const int height = area_.height();
  for (int y = 0; y < height; ++y) {
    GrowLine(&bits_[static_cast<size_t>(y) * static_cast<size_t>(width)],
             width, 1, grow, &reach);
  }
  for (int x = 0; x < width; ++x) {
    GrowLine(&bits_[static_cast<size_t>(x)], height, width, grow, &reach);
  }
}

FollowedRing FollowEdges(const AlphaMask& mask, QPoint start) {
  assert(mask.IsVisible(start));
  assert(!mask.IsVisible(start - QPoint(0, 1)));
  FollowedRing ring;
  QPoint at = start;
  int heading = 0;
  const std::int64_t limit =
      4 * static_cast<std::int64_t>(mask.area().width()) *
          mask.area().height() +
      16;
  bool is_closed = false;
  for (std::int64_t step = 0; step < limit && !is_closed; ++step) {
    const bool is_rightward = heading == 0;
    if (is_rightward) {
      ring.rightward.push_back(at);
    }
    at += kStep[static_cast<size_t>(heading)];
    const auto h = static_cast<size_t>(heading);
    const bool is_left = mask.IsVisible(at + kAheadLeft[h]);
    const bool is_right = mask.IsVisible(at + kAheadRight[h]);
    const int next = is_left ? (heading + 3) % 4
                     : is_right ? heading
                                : (heading + 1) % 4;
    const bool is_turn = next != heading;
    if (is_turn) {
      ring.turns.emplace_back(at);
    }
    heading = next;
    is_closed = at == start && heading == 0;
  }
  return ring;
}

std::vector<std::vector<QPointF>> AllRings(const AlphaMask& mask,
                                           int max_rings) {
  assert(max_rings > 0);
  assert(mask.area().width() <= 3 * kMaxMaskSide);
  std::vector<std::vector<QPointF>> rings;
  const QRect area = mask.area();
  std::vector<std::uint8_t> is_done(
      static_cast<size_t>(area.width()) * static_cast<size_t>(area.height()),
      0);
  const auto index = [&area](QPoint at) {
    return static_cast<size_t>(at.y() - area.top()) *
               static_cast<size_t>(area.width()) +
           static_cast<size_t>(at.x() - area.left());
  };
  for (int y = area.top(); y <= area.bottom(); ++y) {
    for (int x = area.left(); x <= area.right(); ++x) {
      const QPoint at(x, y);
      const bool is_start = mask.IsVisible(at) &&
                            !mask.IsVisible(at - QPoint(0, 1)) &&
                            is_done[index(at)] == 0;
      if (!is_start) {
        continue;
      }
      FollowedRing ring = FollowEdges(mask, at);
      for (const QPoint& corner : ring.rightward) {
        const bool is_on_mask = area.contains(corner);
        if (is_on_mask) {
          is_done[index(corner)] = 1;
        }
      }
      rings.push_back(std::move(ring.turns));
      const bool is_full = static_cast<int>(rings.size()) >= max_rings;
      if (is_full) {
        return rings;
      }
    }
  }
  return rings;
}

}  // namespace snapper
