#include "anim/mirror.h"

#include <QRegularExpression>

#include <array>
#include <optional>
#include <cassert>

#include "anim/warp.h"
#include "anim/warp_shape.h"

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

// The side marker in name and the pattern that found it.
struct SideMatch final {
  QRegularExpressionMatch match;
  const SidePattern* side = nullptr;
};

std::optional<SideMatch> MatchSide(const QString& name) {
  assert(name.size() < 4096);
  assert(!kSides.empty());
  for (const SidePattern& side : kSides) {
    const QRegularExpression pattern(
        QLatin1String(side.pattern),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = pattern.match(name);
    const bool is_sided = match.hasMatch();
    if (is_sided) {
      return SideMatch{std::move(match), &side};
    }
  }
  return std::nullopt;
}

}  // namespace

QString MirrorName(const QString& name) {
  assert(name.size() < 4096);
  const auto found = MatchSide(name);
  if (!found) {
    return name;
  }
  QString mirrored = name;
  const QRegularExpressionMatch& match = found->match;
  mirrored.replace(match.capturedStart(1), match.capturedLength(1),
                   SwapSide(match.captured(1), *found->side));
  assert(mirrored.size() >= name.size() - 1);
  return mirrored;
}

int SideOf(const QString& name) {
  assert(name.size() < 4096);
  const auto found = MatchSide(name);
  if (!found) {
    return 0;
  }
  const bool is_left = found->match.captured(1).compare(
                           QLatin1String(found->side->left),
                           Qt::CaseInsensitive) == 0;
  assert(found->side != nullptr);
  return is_left ? -1 : 1;
}

PiecePose MirrorPose(const PiecePose& pose, const WarpGrid& from,
                     QSize from_size, const WarpGrid& to, QSize to_size) {
  assert(from.columns <= kMaxWarpCells && from.rows <= kMaxWarpCells);
  assert(pose.warp.size() <= static_cast<size_t>(kMaxWarpPoints));
  PiecePose mirrored = pose;
  mirrored.rotation = -pose.rotation;
  mirrored.skew = -pose.skew;
  mirrored.offset.setX(-pose.offset.x());
  const bool has_warp =
      from.IsOn() && to.IsOn() &&
      static_cast<int>(pose.warp.size()) == from.PointCount();
  if (!has_warp) {
    return mirrored;
  }
  const bool is_same_grid = from == to && !from.IsMesh();
  if (is_same_grid) {
    for (int point = 0; point < from.PointCount(); ++point) {
      const QPointF pushed = pose.warp[static_cast<size_t>(point)];
      mirrored.warp[static_cast<size_t>(MirroredPoint(from, point))] =
          QPointF(-pushed.x(), pushed.y());
    }
    return mirrored;
  }
  // Each of to's points takes the push found at its spot mirrored onto
  // from's drawing.
  const std::vector<QPointF> rest = RestPoints(to_size, to);
  const double x = to_size.width() > 0
                       ? from_size.width() /
                             static_cast<double>(to_size.width())
                       : 1.0;
  const double y = to_size.height() > 0
                       ? from_size.height() /
                             static_cast<double>(to_size.height())
                       : 1.0;
  mirrored.warp.assign(rest.size(), QPointF());
  for (size_t i = 0; i < rest.size(); ++i) {
    const QPointF spot((to_size.width() - rest[i].x()) * x,
                       rest[i].y() * y);
    const QPointF pushed = CarriedAt(from, from_size, pose.warp, spot);
    mirrored.warp[i] = QPointF(-pushed.x(), pushed.y());
  }
  return mirrored;
}

PoseMap MirrorPoses(const Doll& doll, const PoseMap& poses) {
  assert(doll.rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(poses.size() <= static_cast<size_t>(kMaxPieceTracks));
  PoseMap mirrored;
  for (const RigPiece& piece : doll.rig.pieces) {
    const QString partner = MirrorName(piece.name);
    const RigPiece* twin =
        partner != piece.name ? FindRig(doll.rig, partner) : nullptr;
    const bool has_partner = twin != nullptr;
    const RigPiece& source = has_partner ? *twin : piece;
    const auto found = poses.find(source.name);
    const PiecePose pose = found == poses.end() ? PiecePose() : found->second;
    mirrored[piece.name] =
        MirrorPose(pose, source.warp, ArtSizeOf(doll, source.name), piece.warp,
                   ArtSizeOf(doll, piece.name));
  }
  return mirrored;
}

}  // namespace snapper
