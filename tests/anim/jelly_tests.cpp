#include <QTest>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <variant>
#include <vector>

#include "anim/doll_drag.h"
#include "anim/jelly.h"
#include "anim/sampler.h"
#include "model/node_physics.h"

namespace snapper {
namespace {

// A 20 by 20 blob on a 2 by 2 grid, every point a jelly node, sitting
// where its points are: knobs as given, no droop.
struct Blob final {
  ClothPiece piece;
  std::vector<QPointF> carried;
};

Blob MakeBlob(double firmness, double wobble, double squish) {
  Blob blob;
  blob.piece.grid = {2, 2};
  for (int row = 0; row <= 2; ++row) {
    for (int column = 0; column <= 2; ++column) {
      blob.carried.push_back(QPointF(column * 10.0, row * 10.0));
    }
  }
  DragNode node;
  node.kind = NodeKind::kJelly;
  node.spring = firmness;
  node.inertia = wobble;
  node.stretch = squish;
  node.gravity = 0.0;
  const ClothSettings settings = ClothSettingsOf(node);
  for (int point = 0; point < 9; ++point) {
    blob.piece.points.push_back(point);
    blob.piece.settings.push_back(settings);
    blob.piece.at.push_back(blob.carried[static_cast<size_t>(point)]);
    blob.piece.speed.push_back(QPointF());
    blob.piece.body_speed.push_back(QPointF());
  }
  blob.piece.cell = 10.0;
  blob.piece.drawn = blob.carried;
  assert(blob.piece.grid.PointCount() == 9);
  assert(ShapeArea(blob.piece.grid, blob.carried) == 400.0);
  return blob;
}

// Where the blob's points are now: every point is a node.
std::vector<QPointF> Now(const Blob& blob) {
  assert(blob.piece.at.size() == 9);
  assert(blob.carried.size() == 9);
  return blob.piece.at;
}

void Run(Blob* blob, int frames) {
  assert(blob != nullptr);
  assert(frames > 0 && frames < 100000);
  for (int frame = 0; frame < frames; ++frame) {
    StepJelly(blob->carried, &blob->piece);
  }
}

double Height(const std::vector<QPointF>& points) {
  assert(!points.empty());
  assert(points.size() < 100000);
  const auto [low, high] = std::minmax_element(
      points.begin(), points.end(),
      [](QPointF a, QPointF b) { return a.y() < b.y(); });
  return high->y() - low->y();
}

// How far the middle point is from halfway between its left and right
// neighbours: 0 when the middle row is straight, as a smooth fit keeps.
double Kink(const std::vector<QPointF>& points) {
  assert(points.size() == 9);
  assert(std::isfinite(points[4].x()));
  const QPointF halfway = (points[3] + points[5]) / 2.0;
  return std::hypot(points[4].x() - halfway.x(), points[4].y() - halfway.y());
}

// A blob with the knobs given, let go after a flick that squashes it
// (sides apart, top and bottom together, so its area and middle stay):
// how far its top right corner is from rest, each frame.
std::vector<double> Swing(double firmness, double wobble, double squish) {
  constexpr int kFrames = 400;
  Blob blob = MakeBlob(firmness, wobble, squish);
  for (size_t point = 0; point < 9; ++point) {
    const double column = static_cast<double>(point % 3) - 1.0;
    const double row = static_cast<double>(point / 3) - 1.0;
    blob.piece.speed[point] = QPointF(column * 2.0, -row * 2.0);
  }
  std::vector<double> swing;
  for (int frame = 0; frame < kFrames; ++frame) {
    StepJelly(blob.carried, &blob.piece);
    swing.push_back(blob.piece.at[2].x() - blob.carried[2].x());
  }
  assert(swing.size() == size_t{kFrames});
  assert(std::isfinite(swing.back()));
  return swing;
}

// The frames from one swing right to the next, on average.
double PeriodOf(const std::vector<double>& swing) {
  assert(swing.size() > 2);
  std::vector<size_t> rises;
  for (size_t i = 1; i < swing.size(); ++i) {
    const bool is_rise = swing[i - 1] < 0.0 && swing[i] >= 0.0;
    if (is_rise) {
      rises.push_back(i);
    }
  }
  assert(rises.size() < swing.size());
  const bool has_two = rises.size() >= 2;
  return has_two ? static_cast<double>(rises.back() - rises.front()) /
                       static_cast<double>(rises.size() - 1)
                 : 0.0;
}

// The frame after which the swing stays under a tenth of its largest.
int SettleOf(const std::vector<double>& swing) {
  assert(!swing.empty());
  const double most = std::abs(*std::max_element(
      swing.begin(), swing.end(),
      [](double a, double b) { return std::abs(a) < std::abs(b); }));
  assert(std::isfinite(most));
  int settle = 0;
  for (size_t i = 0; i < swing.size(); ++i) {
    const bool is_big = std::abs(swing[i]) > most * 0.1;
    if (is_big) {
      settle = static_cast<int>(i) + 1;
    }
  }
  return settle;
}

// The farthest the corner swings.
double Reach(const std::vector<double>& swing) {
  assert(!swing.empty());
  assert(std::isfinite(swing.front()));
  double most = 0.0;
  for (const double x : swing) {
    most = std::max(most, std::abs(x));
  }
  return most;
}

// A 20 by 20 chest on a 2 by 2 grid: the top row hangs it from the
// body, the six points below are jelly.
Doll Chest() {
  Doll doll;
  doll.art.pieces = {{"chest", {"c.png"}, 0, {0, 0}, {20, 20}}};
  doll.rig.pieces = {{"chest", "", {10, 0}, 0, -1, {2, 2}}};
  for (int point = 3; point < 9; ++point) {
    DragNode node{point};
    node.kind = NodeKind::kJelly;
    doll.rig.pieces[0].drag_nodes.push_back(node);
  }
  assert(doll.rig.pieces[0].warp.PointCount() == 9);
  assert(doll.rig.pieces[0].drag_nodes.size() == 6);
  return doll;
}

// The doll stands still, then jumps right at frame 2 and stays.
Layer Hop() {
  Layer layer;
  layer.content = DollLayer{"Chest", {}, false};
  PiecePose moved;
  moved.offset = QPointF(12, 0);
  SetKey(&layer.transform, {Frame(0), PiecePose(), Ease::kStep});
  SetKey(&layer.transform, {Frame(2), moved, Ease::kStep});
  assert(layer.transform.keys.size() == 2);
  assert(std::holds_alternative<DollLayer>(layer.content));
  return layer;
}

double Wobble(const Doll& doll, const Layer& layer, int frame) {
  assert(frame >= 0);
  assert(!doll.rig.pieces.empty());
  const PoseMap poses = DraggedPoses(doll, layer, Frame(frame));
  const auto found = poses.find("chest");
  double most = 0.0;
  const bool has_warp = found != poses.end();
  for (size_t i = 0; has_warp && i < found->second.warp.size(); ++i) {
    const QPointF off = found->second.warp[i];
    most = std::max(most, std::hypot(off.x(), off.y()));
  }
  return most;
}

}  // namespace

class JellyTests : public QObject {
  Q_OBJECT

 private slots:
  void ShapeAreaAddsTheTriangles();
  void SqueezedFromTheSidesItGetsTaller();
  void APokedDotIsSmoothedIntoTheBlob();
  void AtRestItStaysAsDrawn();
  void AChestJigglesAfterAHopAndSettles();
  void FirmnessShortensThePeriodStepByStep();
  void WobbleLengthensTheSettleStepByStep();
  void SquishGrowsTheSwingEvenly();
};

void JellyTests::ShapeAreaAddsTheTriangles() {
  const Blob blob = MakeBlob(0.5, 0.5, 0.5);
  QCOMPARE(ShapeArea(blob.piece.grid, blob.carried), 400.0);
  std::vector<QPointF> wide = blob.carried;
  for (QPointF& point : wide) {
    point.setX(point.x() * 2.0);
  }
  QCOMPARE(ShapeArea(blob.piece.grid, wide), 800.0);
}

void JellyTests::SqueezedFromTheSidesItGetsTaller() {
  Blob blob = MakeBlob(0.5, 0.3, 1.0);
  // A key (or a collider) pinches the blob to half its width.
  for (QPointF& point : blob.carried) {
    point.setX(point.x() * 0.5);
  }
  Run(&blob, 400);
  const double area = ShapeArea(blob.piece.grid, Now(blob));
  QVERIFY(std::abs(area - 400.0) < 400.0 * 0.05);
  QVERIFY(Height(Now(blob)) > 20.0 * 1.5);
}

void JellyTests::APokedDotIsSmoothedIntoTheBlob() {
  Blob blob = MakeBlob(0.5, 0.3, 0.5);
  // One dot flicked on its own would leave a dent in a cloth.
  blob.piece.speed[4] = QPointF(0.0, 4.0);
  Run(&blob, 1);
  const double kicked = Kink(Now(blob));
  QVERIFY(kicked > 1.0);
  Run(&blob, 60);
  QVERIFY(Kink(Now(blob)) < kicked * 0.1);
}

void JellyTests::AtRestItStaysAsDrawn() {
  Blob blob = MakeBlob(0.5, 0.5, 0.5);
  QCOMPARE(blob.piece.at.size(), size_t{9});
  Run(&blob, 50);
  for (size_t i = 0; i < 9; ++i) {
    const QPointF off = blob.piece.at[i] - blob.carried[i];
    QVERIFY(std::hypot(off.x(), off.y()) < 1e-6);
  }
}

void JellyTests::AChestJigglesAfterAHopAndSettles() {
  const Doll doll = Chest();
  const Layer layer = Hop();
  QCOMPARE(Wobble(doll, layer, 1), 0.0);
  QVERIFY(Wobble(doll, layer, 4) > 1.0);
  QVERIFY(Wobble(doll, layer, 300) < 0.05);
  DragTrail trail;
  for (int frame : {0, 6, 7, 40, 3}) {
    QCOMPARE(DraggedPoses(doll, layer, Frame(frame), {}, &trail),
             DraggedPoses(doll, layer, Frame(frame)));
  }
}

void JellyTests::FirmnessShortensThePeriodStepByStep() {
  double last = 1e9;
  for (const double firmness : {0.0, 0.25, 0.5, 0.75, 1.0}) {
    const double period = PeriodOf(Swing(firmness, 1.0, 0.0));
    QVERIFY2(period < last * 0.8, qPrintable(QString::number(period)));
    // About the period the knob promises.
    QVERIFY2(std::abs(period - JellyPeriod(firmness)) <
                 JellyPeriod(firmness) * 0.15,
             qPrintable(QString::number(period)));
    last = period;
  }
  const double slow = PeriodOf(Swing(0.0, 1.0, 0.0));
  QVERIFY(slow > 4.0 * PeriodOf(Swing(1.0, 1.0, 0.0)));
}

void JellyTests::WobbleLengthensTheSettleStepByStep() {
  std::vector<int> settles;
  for (const double wobble : {0.0, 0.2, 0.4, 0.6, 0.8, 1.0}) {
    settles.push_back(SettleOf(Swing(0.5, wobble, 0.0)));
  }
  // Each step of wobble adds about the same few swings.
  for (size_t i = 1; i < settles.size(); ++i) {
    const int step = settles[i] - settles[i - 1];
    QVERIFY2(step > 5 && step < 25,
             qPrintable(QString::number(settles[i - 1]) + " to " +
                        QString::number(settles[i])));
  }
  QVERIFY(settles.back() > 5 * settles.front());
}

void JellyTests::SquishGrowsTheSwingEvenly() {
  const double none = Reach(Swing(0.5, 0.5, 0.0));
  const double half = Reach(Swing(0.5, 0.5, 0.5));
  const double all = Reach(Swing(0.5, 0.5, 1.0));
  const double low = half - none;
  const double high = all - half;
  QVERIFY2(low > 0.0 && high > 0.0 && low > high * 0.6 && high > low * 0.6,
           qPrintable(QString("%1 %2 %3").arg(none).arg(half).arg(all)));
  QVERIFY(all > 1.8 * none);
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::JellyTests)
#include "jelly_tests.moc"
