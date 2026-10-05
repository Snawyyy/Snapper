#include <QJsonArray>
#include <QListWidget>
#include <QPushButton>
#include <QTest>

#include "bench.h"
#include "io/json_file.h"
#include "ui/cast_panel.h"

namespace snapper {
namespace {

// Writes Bob.doll into the bench's library, as the Krita exporter does.
void ExportBob(Bench* bench) {
  const QString folder =
      QDir(bench->library.folder()).filePath("Bob.doll");
  QVERIFY(QDir().mkpath(folder));
  const QJsonObject piece{{"name", "body"},
                          {"drawings", QJsonArray{"body.png"}},
                          {"position", QJsonArray{0, 0}},
                          {"size", QJsonArray{10, 10}}};
  QVERIFY(WriteJsonFile(QDir(folder).filePath("art.json"),
                        QJsonObject{{"format", "snapper-art"},
                                    {"version", 2},
                                    {"canvas", QJsonArray{10, 10}},
                                    {"pieces", QJsonArray{piece}}})
              .has_value());
}

QPushButton* Button(CastPanel* panel, const QString& text) {
  for (QPushButton* button : panel->findChildren<QPushButton*>()) {
    const bool is_match = button->text() == text;
    if (is_match) {
      return button;
    }
  }
  return nullptr;
}

QListWidget* List(CastPanel* panel, int index) {
  return panel->findChildren<QListWidget*>().at(index);
}

}  // namespace

class CastPanelTests final : public QObject {
  Q_OBJECT

 private slots:
  void ImportThenPutOnStage();
  void LayersShowAndHide();
};

void CastPanelTests::ImportThenPutOnStage() {
  Bench bench;
  ExportBob(&bench);
  QVERIFY(bench.shots.Add(-1).has_value());
  CastPanel panel(bench.All());
  QPushButton* import = Button(&panel, "Import");
  QPushButton* place = Button(&panel, "Put on stage");
  QVERIFY(!import->isEnabled());
  QVERIFY(!import->toolTip().isEmpty());
  List(&panel, 0)->setCurrentRow(0);
  QVERIFY(import->isEnabled());
  QVERIFY(!place->isEnabled());
  import->click();
  QVERIFY(FindDoll(bench.history.current(), "Bob") != nullptr);
  QVERIFY(List(&panel, 0)->item(0)->text().contains("in project"));
  QVERIFY(!import->isEnabled());
  QVERIFY(place->isEnabled());
  place->click();
  QCOMPARE(bench.history.current().shots[0]->layers.size(), size_t{1});
  QCOMPARE(List(&panel, 1)->count(), 1);
  QVERIFY(bench.selection.layer().IsValid());
}

void CastPanelTests::LayersShowAndHide() {
  Bench bench;
  const ShotId shot = *bench.shots.Add(-1);
  QVERIFY(bench.stage.AddText(shot, "One").has_value());
  QVERIFY(bench.stage.AddText(shot, "Two").has_value());
  CastPanel panel(bench.All());
  QListWidget* layers = List(&panel, 1);
  QCOMPARE(layers->count(), 2);
  QVERIFY(layers->item(0)->text().startsWith("Two"));
  layers->item(0)->setCheckState(Qt::Unchecked);
  QVERIFY(!bench.history.current().shots[0]->layers[1].is_visible);
  layers->item(1)->setSelected(true);
  QCOMPARE(bench.selection.layer(),
           bench.history.current().shots[0]->layers[0].id);
  Button(&panel, "Up")->click();
  QVERIFY(bench.history.current().shots[0]->layers[1].name == "One");
}

}  // namespace snapper

QTEST_MAIN(snapper::CastPanelTests)
#include "cast_panel_tests.moc"
