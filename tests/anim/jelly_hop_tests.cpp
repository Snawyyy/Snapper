#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <vector>

#include "anim/jelly.h"

namespace snapper {
namespace {

// A hop loop as the feel preview runs it: one hop every kEvery frames,
// up and back down over the first kAir, leaning as it goes.
constexpr int kEvery = 29;
constexpr int kAir = 13;
constexpr double kHeight = 10.0;
constexpr double kLean = 0.5;
constexpr int kPushes = 30;
// A 40 by 40 blob on a 4 by 4 grid: the inner 3 by 3 points are pins
// the body carries (as a doll's undragged dots are), the 16 round the
// edge are jelly.
constexpr int kSide = 4;
constexpr int kPoints = (kSide + 1) * (kSide + 1);
constexpr double kCell = 10.0;

bool IsEdge(int point) {
  assert(point >= 0 && point < kPoints);
  assert(kSide > 2);
  const int row = point / (kSide + 1);
  const int column = point % (kSide + 1);
  return row == 0 || row == kSide || column == 0 || column == kSide;
}

struct Blob final {
  ClothPiece piece;
  std::vector<QPointF> rest;
  std::vector<QPointF> carried;
};

Blob MakeBlob(double firmness, double wobble, double squish) {
  Blob blob;
  blob.piece.grid = {kSide, kSide};
  for (int point = 0; point < kPoints; ++point) {
    blob.rest.push_back(QPointF(point % (kSide + 1) * kCell,
                                point / (kSide + 1) * kCell));
  }
  DragNode node;
  node.kind = NodeKind::kJelly;
  node.spring = firmness;
  node.inertia = wobble;
  node.stretch = squish;
  node.gravity = 0.0;
  for (int point = 0; point < kPoints; ++point) {
    const bool is_edge = IsEdge(point);
    if (is_edge) {
      blob.piece.points.push_back(point);
      blob.piece.settings.push_back(ClothSettingsOf(node));
      blob.piece.at.push_back(blob.rest[static_cast<size_t>(point)]);
    }
  }
  const size_t count = blob.piece.points.size();
  blob.piece.speed.assign(count, QPointF());
  blob.piece.body_speed.assign(count, QPointF());
  blob.piece.cell = kCell;
  blob.piece.drawn = blob.rest;
  blob.carried = blob.rest;
  assert(blob.piece.grid.PointCount() == kPoints);
  assert(count == 16);
  return blob;
}

// The blob's points as the body carries them at frame of the loop.
std::vector<QPointF> Hopped(const std::vector<QPointF>& rest, int frame) {
  assert(rest.size() == static_cast<size_t>(kPoints));
  assert(frame >= 0);
  const int at = frame % kEvery;
  const bool is_flying = at < kAir;
  const double t = static_cast<double>(at) / kAir;
  const double height = is_flying ? kHeight * 4.0 * t * (1.0 - t) : 0.0;
  const double lean =
      is_flying ? kLean * std::sin(std::numbers::pi * t) : 0.0;
  const QPointF middle(kSide * kCell / 2.0, kSide * kCell / 2.0);
  std::vector<QPointF> hopped;
  for (const QPointF& point : rest) {
    const QPointF off = point - middle;
    hopped.push_back(
        middle - QPointF(0.0, height) +
        QPointF(off.x() * std::cos(lean) - off.y() * std::sin(lean),
                off.x() * std::sin(lean) + off.y() * std::cos(lean)));
  }
  return hopped;
}

// The farthest any jelly point strays from where the body carries it
// over kPushes hops; infinity once a point goes bad.
double MostStrayOverHops(double firmness, double wobble, double squish) {
  Blob blob = MakeBlob(firmness, wobble, squish);
  ClothPiece& piece = blob.piece;
  double most = 0.0;
  for (int frame = 0; frame < kPushes * kEvery; ++frame) {
    const std::vector<QPointF> now = Hopped(blob.rest, frame);
    for (size_t i = 0; i < piece.points.size(); ++i) {
      const size_t point = static_cast<size_t>(piece.points[i]);
      piece.body_speed[i] = now[point] - blob.carried[point];
    }
    blob.carried = now;
    StepJelly(blob.carried, &piece);
    for (size_t i = 0; i < piece.points.size(); ++i) {
      const QPointF off =
          piece.at[i] - blob.carried[static_cast<size_t>(piece.points[i])];
      const double stray = std::hypot(off.x(), off.y());
      const bool is_bad = !std::isfinite(stray);
      most = is_bad ? INFINITY : std::max(most, stray);
    }
  }
  assert(most >= 0.0);
  assert(piece.at.size() == 16);
  return most;
}

}  // namespace

class JellyHopTests : public QObject {
  Q_OBJECT

 private slots:
  void HopsOverAndOverStayBounded();
};

void JellyHopTests::HopsOverAndOverStayBounded() {
  // A hop 10 pixels high on a 40 pixel blob: the softest, wobbliest jelly
  // swings wide, but no setting may fling a point more than one and a
  // half blobs from its home. Unbounded, it tore apart (400 and more).
  constexpr double kBound = 60.0;
  QVERIFY(MostStrayOverHops(0.5, 0.5, 1.0) > 0.0);
  for (const double firmness : {0.0, 1.0}) {
    for (const double wobble : {0.0, 1.0}) {
      const double most = MostStrayOverHops(firmness, wobble, 1.0);
      QVERIFY2(most < kBound,
               qPrintable(QString("firmness %1 wobble %2: %3")
                              .arg(firmness)
                              .arg(wobble)
                              .arg(most)));
    }
  }
  QVERIFY(MostStrayOverHops(0.5, 0.5, 1.0) < kBound);
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::JellyHopTests)
#include "jelly_hop_tests.moc"
