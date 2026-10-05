#include "anim/mirror.h"

#include <QRegularExpression>

#include <array>
#include <cassert>

namespace snapper {
namespace {

struct SidePattern final {
  // Matches the side marker as a whole word, prefix or suffix.
  const char* pattern;
  const char* left;
  const char* right;
};

constexpr std::array<SidePattern, 4> kSides = {{
    {"(?<=^|[^a-z])(left|right)(?=$|[^a-z])", "left", "right"},
    {"^(l|r)(?=[_. -])", "l", "r"},
    {"(?<=[_. -])(l|r)$", "l", "r"},
    {"(?<=[_. -])(l|r)(?=[_. -])", "l", "r"},
}};

// Swaps left and right in word, keeping its capitals.
QString SwapSide(const QString& word, const SidePattern& side) {
  assert(!word.isEmpty());
  assert(side.left != nullptr && side.right != nullptr);
  const bool is_left = word.compare(QLatin1String(side.left),
                                    Qt::CaseInsensitive) == 0;
  QString other = QLatin1String(is_left ? side.right : side.left);
  const bool is_upper = word == word.toUpper();
  const bool is_title = !is_upper && word[0].isUpper();
  if (is_upper) {
    return other.toUpper();
  }
  if (is_title) {
    other[0] = other[0].toUpper();
  }
  return other;
}

}  // namespace

QString MirrorName(const QString& name) {
  assert(name.size() < 4096);
  assert(!kSides.empty());
  for (const SidePattern& side : kSides) {
    const QRegularExpression pattern(
        QLatin1String(side.pattern),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(name);
    const bool is_sided = match.hasMatch();
    if (is_sided) {
      QString mirrored = name;
      mirrored.replace(match.capturedStart(1), match.capturedLength(1),
                       SwapSide(match.captured(1), side));
      return mirrored;
    }
  }
  return name;
}

PiecePose MirrorPose(const PiecePose& pose, WarpGrid grid) {
  assert(grid.columns <= kMaxWarpCells && grid.rows <= kMaxWarpCells);
  assert(pose.warp.size() <= static_cast<size_t>(kMaxWarpPoints));
  PiecePose mirrored = pose;
  mirrored.rotation = -pose.rotation;
  mirrored.skew = -pose.skew;
  mirrored.offset.setX(-pose.offset.x());
  const bool has_warp =
      grid.IsOn() && static_cast<int>(pose.warp.size()) == grid.PointCount();
  if (!has_warp) {
    return mirrored;
  }
  const int width = grid.columns + 1;
  for (int row = 0; row <= grid.rows; ++row) {
    for (int column = 0; column < width; ++column) {
      const auto from = static_cast<size_t>(row * width + column);
      const auto to = static_cast<size_t>(row * width + grid.columns - column);
      mirrored.warp[to] = QPointF(-pose.warp[from].x(), pose.warp[from].y());
    }
  }
  return mirrored;
}

PoseMap MirrorPoses(const Doll& doll, const PoseMap& poses) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  PoseMap mirrored;
  for (const RigPiece& piece : doll.rig.pieces) {
    const QString partner = MirrorName(piece.name);
    const bool has_partner =
        partner != piece.name && FindRig(doll.rig, partner) != nullptr;
    const QString& source = has_partner ? partner : piece.name;
    const auto found = poses.find(source);
    const PiecePose pose = found == poses.end() ? PiecePose() : found->second;
    // The partner's grid is assumed the mirror of this one's.
    mirrored[piece.name] = MirrorPose(pose, piece.warp);
  }
  return mirrored;
}

}  // namespace snapper
