#include <QListWidget>
#include <QPushButton>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "ui/cast_panel.h"

namespace snapper {
namespace {

QPushButton* Button(CastPanel* panel, const QString& text) {
  assert(panel != nullptr);
  assert(!text.isEmpty());
  for (QPushButton* button : panel->findChildren<QPushButton*>()) {
    const bool is_match = button->text() == text;
    if (is_match) {
      return button;
    }
  }
  return nullptr;
}

QListWidget* List(CastPanel* panel, int index) {
  assert(panel != nullptr);
  assert(index >= 0);
  return panel->findChild<QListWidget*>(index == 0 ? "characters"
                                                   : "layers");
}

}  // namespace

class CastPanelTests final : public QObject {
  Q_OBJECT

 private slots:
  void LibraryCharactersGoOnStage();
  void LayersShowAndHide();
};

void CastPanelTests::LibraryCharactersGoOnStage() {
  Bench bench;
  // A character made and kept in the library, then left out of the
  // project, as it is in a fresh project.
  QVERIFY(bench.characters.Create("Bob").has_value());
  bench.history.Reset(Project());
  QVERIFY(bench.shots.Add(-1).has_value());
  CastPanel panel(bench.All());
  QPushButton* place = Button(&panel, "Put on stage");
  QPushButton* edit = Button(&panel, "Edit");
  QVERIFY(!place->isEnabled());
  QVERIFY(!place->toolTip().isEmpty());
  QCOMPARE(List(&panel, 0)->count(), 1);
  List(&panel, 0)->selectionModel()->select(
      List(&panel, 0)->model()->index(0, 0), QItemSelectionModel::Select);
  QVERIFY(edit->isEnabled());
  // Putting it on the stage takes it into the project.
  QVERIFY(place->isEnabled());
  place->click();
  QVERIFY(FindCharacter(bench.history.current(), "Bob") != nullptr);
  QCOMPARE(bench.history.current().shots[0]->layers.size(), size_t{1});
  QVERIFY(List(&panel, 0)->item(0)->text().contains("In shot"));
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
  layers->selectionModel()->select(layers->model()->index(1, 0),
                                   QItemSelectionModel::Select);
  QCOMPARE(bench.selection.layer(),
           bench.history.current().shots[0]->layers[0].id);
  Button(&panel, "Up")->click();
  QVERIFY(bench.history.current().shots[0]->layers[1].name == "One");
}

}  // namespace snapper

QTEST_MAIN(snapper::CastPanelTests)
#include "cast_panel_tests.moc"
