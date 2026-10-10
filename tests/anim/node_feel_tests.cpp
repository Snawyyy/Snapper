#include <QTest>

#include <cassert>
#include <cmath>

#include "anim/node_feel.h"
#include "model/node_physics.h"

namespace snapper {

class NodeFeelTests : public QObject {
  Q_OBJECT

 private slots:
  void AFeelReadsBackFromTheNode();
  void FirmAndHeavyGoTheRightWay();
  void EveryKindHasPresetsOnThePad();
  void JellyKnobEndsDiffer();
  void OldJellyTurnsIntoTheNearestKnobs();
};

void NodeFeelTests::AFeelReadsBackFromTheNode() {
  for (const NodeKind kind :
       {NodeKind::kDrag, NodeKind::kCloth, NodeKind::kJelly}) {
    DragNode node{7};
    node.kind = kind;
    const DragNode felt = WithFeel(node, {0.25, 0.75});
    QCOMPARE(felt.point, 7);
    QCOMPARE(felt.kind, kind);
    const Feel back = FeelOf(felt);
    QVERIFY(std::abs(back.firm - 0.25) < 1e-9);
    QVERIFY(std::abs(back.heavy - 0.75) < 1e-9);
  }
}

void NodeFeelTests::FirmAndHeavyGoTheRightWay() {
  DragNode jelly;
  jelly.kind = NodeKind::kJelly;
  const DragNode soft = WithFeel(jelly, {0.0, 0.0});
  const DragNode firm = WithFeel(jelly, {1.0, 1.0});
  // Firmer springs back harder and squishes less; heavier swings more.
  QVERIFY(firm.spring > soft.spring);
  QVERIFY(firm.stretch < soft.stretch);
  QVERIFY(firm.inertia > soft.inertia);
  QVERIFY(firm.gravity > soft.gravity);
  // Jelly has no air; cloth settles sooner when light.
  QCOMPARE(firm.air, jelly.air);
  DragNode cloth;
  cloth.kind = NodeKind::kCloth;
  QVERIFY(WithFeel(cloth, {1.0, 1.0}).air < WithFeel(cloth, {0.0, 0.0}).air);
  DragNode drag;
  const DragNode snappy = WithFeel(drag, {1.0, 0.0});
  QVERIFY(snappy.lag < WithFeel(drag, {0.0, 0.0}).lag);
  // Out of range is kept on the pad.
  QCOMPARE(WithFeel(jelly, {3.0, -1.0}), WithFeel(jelly, {1.0, 0.0}));
}

void NodeFeelTests::EveryKindHasPresetsOnThePad() {
  for (const NodeKind kind :
       {NodeKind::kDrag, NodeKind::kCloth, NodeKind::kJelly}) {
    const std::vector<FeelPreset> presets = FeelPresets(kind);
    QCOMPARE(presets.size(), size_t{3});
    for (const FeelPreset& preset : presets) {
      QVERIFY(!preset.name.isEmpty());
      QVERIFY(preset.feel.firm >= 0.0 && preset.feel.firm <= 1.0);
      QVERIFY(preset.feel.heavy >= 0.0 && preset.feel.heavy <= 1.0);
    }
  }
  QCOMPARE(FeelPresets(NodeKind::kJelly)[0].name, QString("Pudding"));
}

// The jelly knobs reach their promised ends, and a stiffer or
// longer-ringing old node turns into a firmer or wobblier one.
void NodeFeelTests::JellyKnobEndsDiffer() {
  QVERIFY(std::abs(JellyPeriod(0.0) - 24.0) < 1e-9);
  QVERIFY(std::abs(JellyPeriod(1.0) - 4.0) < 1e-9);
  // Geometric: each half is the same ratio.
  QVERIFY(std::abs(JellyPeriod(0.0) / JellyPeriod(0.5) -
                   JellyPeriod(0.5) / JellyPeriod(1.0)) < 1e-9);
  QCOMPARE(JellyJiggles(0.0), 0.0);
  QCOMPARE(JellyJiggles(1.0), 6.0);
  DragNode jelly;
  jelly.kind = NodeKind::kJelly;
  jelly.spring = 0.0;
  const JellyMotion soft = JellyMotionOf(jelly);
  jelly.spring = 1.0;
  QVERIFY(JellyMotionOf(jelly).pull > 5.0 * soft.pull);
  jelly.gravity = 0.0;
  QCOMPARE(JellyMotionOf(jelly).fall, 0.0);
  jelly.gravity = 0.5;
  QVERIFY(JellyMotionOf(jelly).fall > 0.0);
}

void NodeFeelTests::OldJellyTurnsIntoTheNearestKnobs() {
  DragNode old;
  old.kind = NodeKind::kJelly;
  old.stretch = 0.0;
  old.spring = 0.4;
  old.inertia = 0.7;
  old.air = 0.3;
  old.gravity = 0.5;
  const DragNode knobs = JellyFromOld(old);
  QCOMPARE(knobs.stretch, 0.0);
  QCOMPARE(knobs.air, DragNode().air);
  QVERIFY(knobs.spring > 0.0 && knobs.spring < 1.0);
  QVERIFY(knobs.gravity > 0.0 && knobs.gravity < 1.0);
  DragNode stiff = old;
  stiff.spring = 0.9;
  QVERIFY(JellyFromOld(stiff).spring > knobs.spring);
  DragNode ringing = old;
  ringing.inertia = 1.0;
  ringing.air = 0.0;
  QVERIFY(JellyFromOld(ringing).inertia > knobs.inertia);
  DragNode squishy = old;
  squishy.stretch = 1.0;
  QCOMPARE(JellyFromOld(squishy).stretch, 1.0);
  // Its period matches the old one's: an old spring of 0.9 rang about
  // every 10 frames.
  const double period = JellyPeriod(JellyFromOld(stiff).spring);
  QVERIFY2(period > 6.0 && period < 11.0, qPrintable(QString::number(period)));
}

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::NodeFeelTests)
#include "node_feel_tests.moc"
