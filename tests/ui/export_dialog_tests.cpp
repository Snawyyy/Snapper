#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include "bench.h"
#include "ui/disc_burner.h"
#include "ui/export_dialog.h"
#include "ui/theme.h"

namespace snapper {

class ExportDialogTests final : public QObject {
  Q_OBJECT

 private slots:
  void SaysWhyThenExports();
};

void ExportDialogTests::SaysWhyThenExports() {
  Bench bench;
  ExportDialog dialog(bench.All(), nullptr);
  QVERIFY(dialog.WhyNotReady().contains("shot"));
  Project project;
  project.canvas = {64, 48};
  Shot shot;
  shot.id = ShotId(1);
  shot.length = Frame(6);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench.history.Reset(project);
  QVERIFY(dialog.WhyNotReady().contains("file"));
  const QString path = QDir(bench.dir.path()).filePath("out.gif");
  dialog.SetPath(path);
  QVERIFY(dialog.WhyNotReady().isEmpty());
  QSignalSpy done(&bench.exporter, &ExportManager::Finished);
  QVERIFY(bench.exporter.Start(dialog.Request()).has_value());
  QVERIFY(done.wait(10000));
  QVERIFY(QFile::exists(path));
  // The burned disc stops and wears the file's name.
  auto* burner = dialog.findChild<DiscBurner*>();
  QVERIFY(burner != nullptr);
  QCOMPARE(burner->state(), DiscBurner::State::kDone);
  const QString shots = qEnvironmentVariable("SNAPPER_SHOTS");
  if (!shots.isEmpty()) {
    dialog.resize(420, 520);
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    dialog.grab().save(shots + "/21_burner.png");
  }
}

}  // namespace snapper

QTEST_MAIN(snapper::ExportDialogTests)
#include "export_dialog_tests.moc"
