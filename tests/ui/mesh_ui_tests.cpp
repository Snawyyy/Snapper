#include <QCheckBox>
#include <QComboBox>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "ui/rig_panel.h"

namespace snapper {
namespace {

// A round blob on disk as Bob's only piece, warped by an old 2 by 2
// grid with a drag node on its middle dot.
void Stage(Bench* bench) {
  assert(bench != nullptr);
  QImage blob(80, 80, QImage::Format_ARGB32);
  blob.fill(Qt::transparent);
  {
    QPainter painter(&blob);
    painter.setBrush(Qt::red);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QRectF(4, 4, 72, 72));
  }
  QVERIFY(blob.save(QDir(bench->dir.path()).filePath("blob.png")));
  Doll doll;
  doll.folder = bench->dir.path();
  doll.art.canvas = QSize(200, 200);
  doll.art.pieces = {{"blob", {"blob.png"}, 0, {-40, -40}, {80, 80}}};
  RigPiece rig{"blob", "", {40, 40}, 0, -1, WarpGrid{2, 2}};
  rig.drag_nodes = {DragNode{4}};
  doll.rig.pieces = {rig};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  bench->history.Reset(project);
}

const RigPiece& Blob(const Bench& bench) {
  assert(FindDoll(bench.history.current(), "Bob") != nullptr);
  assert(!bench.dir.path().isEmpty());
  return *FindRig(FindDoll(bench.history.current(), "Bob")->rig, "blob");
}

}  // namespace

class MeshUiTests : public QObject {
  Q_OBJECT

 private slots:
  void OldGridOffersConvertAndGreysDetail();
  void ConvertKeepsTheDragNodeAndSaysSo();
  void DetailRemakesTheMeshAsOneStep();
};

void MeshUiTests::OldGridOffersConvertAndGreysDetail() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  panel.PickPiece("blob");
  auto* convert = panel.findChild<QPushButton*>("convert_mesh");
  auto* detail = panel.findChild<QComboBox*>("mesh_detail");
  QVERIFY(convert != nullptr && detail != nullptr);
  QVERIFY(convert->isEnabled());
  QVERIFY(!detail->isEnabled());
  QVERIFY(detail->toolTip().contains("old grid"));
}

void MeshUiTests::ConvertKeepsTheDragNodeAndSaysSo() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  panel.PickPiece("blob");
  panel.findChild<QPushButton*>("convert_mesh")->click();
  const RigPiece& blob = Blob(bench);
  QVERIFY(blob.warp.IsMesh());
  QCOMPARE(blob.drag_nodes.size(), size_t{1});
  const QString said = bench.toasts.queue().toasts().back().text;
  QVERIFY2(said.contains("1 dot's settings carried over"), qPrintable(said));
  auto* convert = panel.findChild<QPushButton*>("convert_mesh");
  QVERIFY(!convert->isEnabled());
  QVERIFY(!panel.findChildren<QSpinBox*>().empty());
  QVERIFY(panel.findChild<QComboBox*>("mesh_detail")->isEnabled());
}

void MeshUiTests::DetailRemakesTheMeshAsOneStep() {
  Bench bench;
  Stage(&bench);
  RigPanel panel(bench.All());
  panel.PickPiece("blob");
  panel.findChild<QPushButton*>("convert_mesh")->click();
  const int normal = Blob(bench).warp.PointCount();
  auto* detail = panel.findChild<QComboBox*>("mesh_detail");
  QVERIFY(detail != nullptr);
  detail->setCurrentIndex(static_cast<int>(MeshDetail::kMany));
  emit detail->activated(detail->currentIndex());
  QVERIFY(Blob(bench).warp.PointCount() > normal);
  QCOMPARE(Blob(bench).warp.mesh->detail, MeshDetail::kMany);
  bench.history.Undo();
  QCOMPARE(Blob(bench).warp.PointCount(), normal);
}

}  // namespace snapper

QTEST_MAIN(snapper::MeshUiTests)
#include "mesh_ui_tests.moc"
