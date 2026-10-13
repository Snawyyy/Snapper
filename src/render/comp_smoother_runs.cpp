#include "render/comp_smoother_runs.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

namespace snapper {
namespace {

// The longest crossing edge (the riser of a step) still read as a stair
// step; longer ones are the sides of shapes.
constexpr int kMaxRiser = 32;
// The next run of a step must be at least this part of a run's length
// for the two to be read as one slope.
constexpr int kRunRatio = 4;
// Runs shorter than this get Extra Smooth in full, fading out up to it
// (SMAA's smooth distance for U shapes, used for every shape here).
constexpr double kExtraReach = 32.0;
// RGB distance within which a pixel counts as a picked line color.
constexpr double kLineTolerance = 40.0;
// Largest picture side the search walks.
constexpr int kMaxSide = 1 << 15;

int Channel(QRgb pixel, int shift) {
  assert(shift >= 0 && shift <= 24);
  assert(shift % 8 == 0);
  return static_cast<int>((pixel >> shift) & 0xFFu);
}

// The largest channel step between a and b, alpha included.
int Distance(QRgb a, QRgb b) {
  assert(sizeof(QRgb) == 4);
  int most = 0;
  for (int shift = 0; shift <= 24; shift += 8) {
    most = std::max(most, std::abs(Channel(a, shift) - Channel(b, shift)));
  }
  assert(most >= 0 && most <= 255);
  return most;
}

bool IsNear(QRgb pixel, const QColor& color) {
  assert(color.isValid());
  assert(kLineTolerance > 0.0);
  const bool is_clear = qAlpha(pixel) == 0;
  if (is_clear) {
    return false;
  }
  const double dr = qRed(pixel) - color.red();
  const double dg = qGreen(pixel) - color.green();
  const double db = qBlue(pixel) - color.blue();
  return std::sqrt(dr * dr + dg * dg + db * db) <= kLineTolerance;
}

// One direction's search. u runs along the edges, v across them: for
// the horizontal pass u is x and the edge of line v lies between rows v
// and v + 1; the vertical pass swaps them. The near pixel of an edge is
// on line v, the far one on v + 1.
class EdgePass final {
 public:
  EdgePass(const CelGrid& cel, const SmootherEdgeRule& rule,
           bool is_vertical, SmootherShares* shares)
      : cel_(cel), rule_(rule), is_vertical_(is_vertical), shares_(shares) {
    assert(shares_ != nullptr && cel_.width > 0 && cel_.height > 0);
    assert(shares_->size() == cel_.pixels.size());
    along_ = is_vertical_ ? cel_.height : cel_.width;
    across_ = is_vertical_ ? cel_.width : cel_.height;
  }

  void Run() {
    assert(along_ <= kMaxSide && across_ <= kMaxSide);
    assert(along_ > 0);
    for (int v = 0; v + 1 < across_; ++v) {
      RunLine(v);
    }
  }

 private:
  bool Has(int u, int v) const {
    return u >= 0 && u < along_ && v >= 0 && v < across_;
  }

  size_t IndexOf(int u, int v) const {
    assert(Has(u, v));
    const int x = is_vertical_ ? v : u;
    const int y = is_vertical_ ? u : v;
    assert(x < cel_.width && y < cel_.height);
    return static_cast<size_t>(y) * static_cast<size_t>(cel_.width) +
           static_cast<size_t>(x);
  }

  QRgb At(int u, int v) const { return cel_.pixels[IndexOf(u, v)]; }

  bool Differs(QRgb a, QRgb b) const { return Distance(a, b) > rule_.range; }

  bool IsWanted(QRgb a, QRgb b) const {
    const bool is_any = rule_.line_colors.empty();
    if (is_any) {
      return true;
    }
    assert(rule_.line_colors.size() < 4096);
    return std::ranges::any_of(rule_.line_colors, [&](const QColor& line) {
      return IsNear(a, line) || IsNear(b, line);
    });
  }

  bool IsEdge(int u, int v) const {
    assert(Has(u, v) && Has(u, v + 1));
    const QRgb near = At(u, v);
    const QRgb far = At(u, v + 1);
    return Differs(near, far) && IsWanted(near, far);
  }

  // True when both lines change color between u - 1 and u: the edge
  // flips there (a thin line stepping), so its run ends.
  bool Splits(int u, int v) const {
    assert(u > 0);
    return Differs(At(u - 1, v), At(u, v)) &&
           Differs(At(u - 1, v + 1), At(u, v + 1));
  }

  void RunLine(int v) {
    assert(v >= 0 && v + 1 < across_);
    int start = -1;
    for (int u = 0; u <= along_; ++u) {
      const bool is_edge = u < along_ && IsEdge(u, v);
      const bool is_more = is_edge && start >= 0 && !Splits(u, v);
      if (is_more) {
        continue;
      }
      const bool has_run = start >= 0;
      if (has_run) {
        BlendRun(start, u - start, v);
      }
      start = is_edge ? u : -1;
    }
  }

  // True when the run's end at u_end turns, going outward (-1 or +1
  // along u), into the next step of a staircase on the side dir (-1 the
  // near line's side, +1 the far one's): the color across reaches past
  // the end on that line, the riser is at most length long, and past it
  // the edge goes on outward.
  bool IsStep(int u_end, int v, int outward, int dir, int length) const {
    assert(outward == -1 || outward == 1);
    assert(dir == -1 || dir == 1);
    const int u_out = u_end + outward;
    const int first = dir < 0 ? v : v + 1;
    const int other = dir < 0 ? v + 1 : v;
    const bool is_turning = !Differs(At(u_out, first), At(u_end, other));
    if (!is_turning) {
      return false;
    }
    const int limit = std::min(length, kMaxRiser);
    for (int k = 0; k < limit; ++k) {
      const int line = first + dir * k;
      const int next = line + dir;
      const bool is_riser =
          Has(u_out, next) && Differs(At(u_out, line), At(u_end, line));
      if (!is_riser) {
        return false;
      }
      const bool goes_on = Differs(At(u_out, line), At(u_out, next));
      if (goes_on) {
        return IsNextRunLong(u_out, line, next, outward, length);
      }
    }
    return false;
  }

  // True when the edge between lines line and next, from u_out going
  // outward, is long enough next to a run of length to be the next step
  // of one slope. A one pixel notch at the corner of a box is not: its
  // next run turns at once into the box's side, and smoothing it would
  // smear the box's edges.
  bool IsNextRunLong(int u_out, int line, int next, int outward,
                     int length) const {
    assert(Has(u_out, line) && Has(u_out, next));
    assert(length > 0);
    const int needed =
        std::min((length + kRunRatio - 1) / kRunRatio, kMaxRiser);
    for (int j = 1; j < needed; ++j) {
      const int u = u_out + outward * j;
      const bool is_edge =
          Has(u, line) && Differs(At(u, line), At(u, next));
      if (!is_edge) {
        return false;
      }
    }
    return true;
  }

  // +1 when the line rises into the near pixels at this end, -1 when it
  // dips into the far ones, 0 when the end is no step.
  int EndSign(int u_end, int v, int outward, int length) const {
    assert(length > 0);
    assert(outward == -1 || outward == 1);
    const bool is_inside = Has(u_end + outward, v);
    if (!is_inside) {
      return 0;
    }
    const bool rises = IsStep(u_end, v, outward, -1, length);
    const bool dips = IsStep(u_end, v, outward, 1, length);
    return rises && !dips ? 1 : dips && !rises ? -1 : 0;
  }

  void BlendRun(int start, int length, int v) {
    assert(start >= 0 && length > 0 && start + length <= along_);
    assert(v >= 0 && v + 1 < across_);
    const int left = EndSign(start, v, -1, length);
    const int right = EndSign(start + length - 1, v, 1, length);
    const bool is_flat = left == 0 && right == 0;
    if (is_flat) {
      return;
    }
    // The line runs from height sign / 2 at an end (the middle of the
    // riser) to 0 at the middle of the run.
    const double half = length / 2.0;
    const auto height_left = [&](double x) {
      return left * 0.5 * (1.0 - x / half);
    };
    const auto height_right = [&](double x) {
      return right * 0.5 * (1.0 - (length - x) / half);
    };
    for (int i = 0; i < length; ++i) {
      const double a = i;
      const double b = i + 1.0;
      const bool has_left = left != 0 && a < half;
      if (has_left) {
        const double end = std::min(b, half);
        Add(start + i, v, length,
            (end - a) * (height_left(a) + height_left(end)) / 2.0);
      }
      const bool has_right = right != 0 && b > half;
      if (has_right) {
        const double begin = std::max(a, half);
        Add(start + i, v, length,
            (b - begin) * (height_right(begin) + height_right(b)) / 2.0);
      }
    }
  }

  // area > 0: the near pixel at u takes that much of the far one; < 0
  // the far pixel takes of the near one.
  void Add(int u, int v, int length, double area) {
    assert(std::abs(area) <= 0.5 + 1e-9);
    assert(length > 0);
    const double plain = std::abs(area);
    const double fade = 1.0 - std::min(1.0, length / kExtraReach);
    const double smooth = std::sqrt(plain * 2.0) * 0.5;
    const double share =
        plain + (smooth - plain) * std::clamp(rule_.extra, 0.0, 1.0) * fade;
    const bool is_near = area > 0.0;
    const int taker = is_near ? v : v + 1;
    const SmootherSide side =
        is_vertical_ ? (is_near ? SmootherSide::kRight : SmootherSide::kLeft)
                     : (is_near ? SmootherSide::kDown : SmootherSide::kUp);
    (*shares_)[IndexOf(u, taker)][static_cast<size_t>(side)] +=
        static_cast<float>(share);
  }

  const CelGrid& cel_;
  const SmootherEdgeRule& rule_;
  bool is_vertical_;
  // Not owned.
  SmootherShares* shares_;
  int along_ = 0;
  int across_ = 0;
};

}  // namespace

SmootherShares FindSmootherShares(const CelGrid& cel,
                                  const SmootherEdgeRule& rule) {
  assert(cel.pixels.size() ==
         static_cast<size_t>(cel.width) * static_cast<size_t>(cel.height));
  assert(rule.range >= 0 && rule.range <= 255);
  SmootherShares shares(cel.pixels.size(), {0.0f, 0.0f, 0.0f, 0.0f});
  const bool is_empty = cel.pixels.empty();
  if (is_empty) {
    return shares;
  }
  EdgePass(cel, rule, false, &shares).Run();
  EdgePass(cel, rule, true, &shares).Run();
  return shares;
}

}  // namespace snapper
