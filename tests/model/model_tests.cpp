#include <QTest>

#include "model/project.h"

namespace snapper {

class ModelTests final : public QObject {
  Q_OBJECT

 private slots:
  void DefaultProjectIsFullHd();
  void CanvasMustBeEvenAndInRange();
};

void ModelTests::DefaultProjectIsFullHd() {
  const Project project;
  QCOMPARE(project.canvas, (CanvasSize{1920, 1080}));
  QVERIFY(IsValidCanvas(project.canvas));
  QVERIFY(!project.name.isEmpty());
}

void ModelTests::CanvasMustBeEvenAndInRange() {
  QVERIFY(IsValidCanvas({1080, 1920}));
  QVERIFY(IsValidCanvas({kMinCanvasSide, kMaxCanvasSide}));
  QVERIFY(!IsValidCanvas({1921, 1080}));
  QVERIFY(!IsValidCanvas({kMinCanvasSide - 2, 1080}));
  QVERIFY(!IsValidCanvas({1920, kMaxCanvasSide + 2}));
  QVERIFY(!IsValidCanvas({0, 0}));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ModelTests)
#include "model_tests.moc"
