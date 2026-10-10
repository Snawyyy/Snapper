#include <QImage>
#include <QPainter>
#include <QTest>

#include <cassert>
#include <cmath>
#include <vector>

#include "ui/dot_kinds.h"
#include "ui/feel_pad.h"
#include "ui/material_art.h"
#include "ui/wobble_blob.h"

namespace snapper {

class DotKindsTests final : public QObject {
  Q_OBJECT

 private slots:
  void MixCountsWhatThePickedDotsAre();
  void RowsFollowTheKindsInUse();
  void SquishOnlyWhenJellyAloneUsesStretch();
  void TitleNamesThePieceAndSaysMixed();
  void DistinctFeelsMergeNearOnes();
  void PadMathRoundTrips();
  void WobbleSettlesAndHeavyRingsWider();
  void EveryMaterialPaintsAtEveryPhase();
};

namespace {

DragNode Node(NodeKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kNodeKindCount);
  DragNode node;
  node.kind = kind;
  return node;
}

}  // namespace

void DotKindsTests::MixCountsWhatThePickedDotsAre() {
  const DragNode drag = Node(NodeKind::kDrag);
  const DragNode jelly = Node(NodeKind::kJelly);
  const KindMix none(std::vector<const DragNode*>{});
  QCOMPARE(none.Chosen(), Material::kStill);
  QVERIFY(!none.IsMixed());
  const KindMix jellies({&jelly, &jelly});
  QCOMPARE(jellies.Chosen(), Material::kJelly);
  QCOMPARE(jellies.ShareOf(Material::kJelly), Share::kAll);
  QCOMPARE(jellies.ShareOf(Material::kDrag), Share::kNone);
  const KindMix mixed({&drag, nullptr, &jelly, &jelly});
  QVERIFY(mixed.IsMixed());
  QCOMPARE(mixed.Chosen(), Material::kStill);
  QCOMPARE(mixed.ShareOf(Material::kJelly), Share::kSome);
  QCOMPARE(mixed.ShareOf(Material::kStill), Share::kSome);
  QCOMPARE(mixed.CountOf(Material::kJelly), 2);
  const KindMix plain({nullptr, nullptr});
  QCOMPARE(plain.Chosen(), Material::kStill);
  QCOMPARE(plain.ShareOf(Material::kStill), Share::kAll);
}

void DotKindsTests::RowsFollowTheKindsInUse() {
  const DragNode drag = Node(NodeKind::kDrag);
  const DragNode cloth = Node(NodeKind::kCloth);
  const DragNode jelly = Node(NodeKind::kJelly);
  const std::vector<DragField> drag_rows = {DragField::kLag,
                                            DragField::kBounce};
  QCOMPARE(FineRowsOf(KindMix({&drag})), drag_rows);
  const std::vector<DragField> soft_rows = {
      DragField::kStretch, DragField::kSpring, DragField::kInertia,
      DragField::kAir, DragField::kGravity};
  QCOMPARE(FineRowsOf(KindMix({&cloth})), soft_rows);
  // Jelly has no air.
  const std::vector<DragField> jelly_rows = {
      DragField::kStretch, DragField::kSpring, DragField::kInertia,
      DragField::kGravity};
  QCOMPARE(FineRowsOf(KindMix({&jelly})), jelly_rows);
  QCOMPARE(FineRowsOf(KindMix({&cloth, &jelly})), soft_rows);
  QCOMPARE(FineRowsOf(KindMix({&drag, &jelly})).size(), size_t{6});
  QVERIFY(FineRowsOf(KindMix({nullptr})).empty());
  QVERIFY(RowAppliesTo(DragField::kLag, Material::kDrag));
  QVERIFY(!RowAppliesTo(DragField::kLag, Material::kJelly));
  QVERIFY(!RowAppliesTo(DragField::kGravity, Material::kStill));
}

void DotKindsTests::SquishOnlyWhenJellyAloneUsesStretch() {
  const DragNode cloth = Node(NodeKind::kCloth);
  const DragNode jelly = Node(NodeKind::kJelly);
  QCOMPARE(FineLabel(DragField::kStretch, KindMix({&jelly})),
           QString("Squish"));
  QCOMPARE(FineLabel(DragField::kStretch, KindMix({&cloth})),
           QString("Stretch"));
  QCOMPARE(FineLabel(DragField::kStretch, KindMix({&cloth, &jelly})),
           QString("Stretch"));
  // Jelly alone shows its four knobs.
  QCOMPARE(FineLabel(DragField::kSpring, KindMix({&jelly})),
           QString("Firmness"));
  QCOMPARE(FineLabel(DragField::kInertia, KindMix({&jelly})),
           QString("Wobble"));
  QCOMPARE(FineLabel(DragField::kGravity, KindMix({&jelly})),
           QString("Droop"));
  QCOMPARE(FineLabel(DragField::kSpring, KindMix({&cloth, &jelly})),
           QString("Bounce"));
  QVERIFY(FineHelp(DragField::kStretch, KindMix({&jelly}))
              .contains("squashes"));
  QVERIFY(FineHelp(DragField::kInertia, KindMix({&jelly}))
              .contains("jiggles"));
}

void DotKindsTests::TitleNamesThePieceAndSaysMixed() {
  const DragNode drag = Node(NodeKind::kDrag);
  QCOMPARE(TitleOf({}, KindMix(std::vector<const DragNode*>{})),
           QString("Warp dots"));
  QCOMPARE(TitleOf({"Chest"}, KindMix({&drag})),
           QString("Warp dot: Chest"));
  QCOMPARE(TitleOf({"Chest", "Chest", "Chest"},
                   KindMix({&drag, &drag, &drag})),
           QString("Warp dots: Chest, 3 picked"));
  QCOMPARE(TitleOf({"Chest", "Head"}, KindMix({&drag, &drag})),
           QString("Warp dots: 2 pieces, 2 picked"));
  QCOMPARE(TitleOf({"Chest", "Chest"}, KindMix({&drag, nullptr})),
           QString("Warp dots: Chest, 2 picked, Mixed"));
}

void DotKindsTests::DistinctFeelsMergeNearOnes() {
  DragNode a = Node(NodeKind::kJelly);
  DragNode b = WithFeel(a, {0.3, 0.5});
  a = WithFeel(a, {0.3, 0.5});
  const DragNode c = WithFeel(a, {0.9, 0.1});
  QCOMPARE(DistinctFeels({&a, &b}).size(), size_t{1});
  QCOMPARE(DistinctFeels({&a, &b, &c}).size(), size_t{2});
  QVERIFY(SameFeel({0.5, 0.5}, {0.51, 0.49}));
  QVERIFY(!SameFeel({0.5, 0.5}, {0.6, 0.5}));
}

void DotKindsTests::PadMathRoundTrips() {
  const QRectF area(10, 20, 100, 200);
  // Top is heavy, right is firm.
  QCOMPARE(FeelAt(QPointF(110, 20), area), (Feel{1.0, 1.0}));
  QCOMPARE(FeelAt(QPointF(10, 220), area), (Feel{0.0, 0.0}));
  // Outside is held to the edge.
  QCOMPARE(FeelAt(QPointF(500, -50), area), (Feel{1.0, 1.0}));
  const Feel feel{0.25, 0.75};
  QCOMPARE(PointOf(feel, area), QPointF(35, 70));
  QCOMPARE(FeelAt(PointOf(feel, area), area), feel);
}

void DotKindsTests::WobbleSettlesAndHeavyRingsWider() {
  const Feel light{0.5, 0.0};
  const Feel heavy{0.5, 1.0};
  QVERIFY(std::abs(WobbleAt(light, 0.0)) <= 1.0);
  QVERIFY(WobbleAt(heavy, 0.0) > WobbleAt(light, 0.0));
  // Late in the loop it has all but stopped.
  QVERIFY(std::abs(WobbleAt(heavy, 2.3)) < 0.05);
  // Every loop starts the same kick again.
  QCOMPARE(WobbleAt(heavy, 0.3), WobbleAt(heavy, 0.3 + kWobbleLoopS));
  // Deterministic.
  QCOMPARE(WobbleAt(light, 0.7), WobbleAt(light, 0.7));
}

void DotKindsTests::EveryMaterialPaintsAtEveryPhase() {
  QImage image(64, 48, QImage::Format_ARGB32);
  QCOMPARE(Swing(0.0), 0.0);
  QVERIFY(std::abs(Swing(1.0)) < 1e-9);
  for (int m = 0; m < kMaterialCount; ++m) {
    image.fill(Qt::black);
    QPainter painter(&image);
    for (int step = 0; step < 8; ++step) {
      PaintMaterial(&painter, QRectF(0, 0, 64, 48), static_cast<Material>(m),
                    step / 8.0, Qt::white);
    }
    painter.end();
    // Something was drawn.
    QVERIFY(image != QImage(64, 48, QImage::Format_ARGB32));
    bool has_ink = false;
    for (int y = 0; y < 48 && !has_ink; ++y) {
      for (int x = 0; x < 64 && !has_ink; ++x) {
        has_ink = image.pixelColor(x, y) != QColor(Qt::black);
      }
    }
    QVERIFY(has_ink);
  }
}

}  // namespace snapper

QTEST_MAIN(snapper::DotKindsTests)
#include "dot_kinds_tests.moc"
