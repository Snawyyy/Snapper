#include <QTest>

#include <cassert>

#include "anim/node_feel.h"

namespace snapper {

class NodeFeelTests : public QObject {
  Q_OBJECT

 private slots:
  void AFeelReadsBackFromTheNode();
  void FirmAndHeavyGoTheRightWay();
  void EveryKindHasPresetsOnThePad();
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
  QVERIFY(firm.air < soft.air);
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

}  // namespace snapper

QTEST_GUILESS_MAIN(snapper::NodeFeelTests)
#include "node_feel_tests.moc"
