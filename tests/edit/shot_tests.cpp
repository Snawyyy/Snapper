#include <QTest>

#include "edit/history_manager.h"
#include "edit/shot_manager.h"

namespace snapper {
namespace {

QStringList Names(const Project& project) {
  QStringList names;
  for (const auto& shot : project.shots) {
    names.append(shot->name);
  }
  return names;
}

}  // namespace

class ShotTests final : public QObject {
  Q_OBJECT

 private slots:
  void AddInsertsWithFreshIds();
  void DuplicateCopiesWithFreshIds();
  void MoveAndRemoveReorder();
  void SettingsCheckTheirValues();
};

void ShotTests::AddInsertsWithFreshIds() {
  HistoryManager history{Project()};
  ShotManager shots(&history);
  const auto first = shots.Add(-1);
  const auto second = shots.Add(0);
  QVERIFY(first.has_value() && second.has_value());
  QVERIFY(*first != *second);
  QCOMPARE(Names(history.current()), QStringList({"Shot 2", "Shot 1"}));
  QCOMPARE(history.current().shots[0]->length, Frame(48));
  history.Undo();
  QCOMPARE(Names(history.current()), QStringList({"Shot 1"}));
  // The id counter is part of the project, so undo hands the undone
  // shot's id out again; nothing can still point at it.
  QCOMPARE(*shots.Add(99), ShotId(2));
}

void ShotTests::DuplicateCopiesWithFreshIds() {
  Project project;
  Shot shot;
  shot.id = ShotId(1);
  shot.name = "Intro";
  Layer layer;
  layer.id = LayerId(1);
  shot.layers = {layer};
  project.shots = {std::make_shared<const Shot>(shot)};
  project.next_shot_id = 2;
  project.next_layer_id = 2;
  HistoryManager history(project);
  ShotManager shots(&history);
  const auto copy = shots.Duplicate(ShotId(1));
  QVERIFY(copy.has_value());
  QCOMPARE(Names(history.current()), QStringList({"Intro", "Intro copy"}));
  QCOMPARE(history.current().shots[1]->layers[0].id, LayerId(2));
  QVERIFY(!shots.Duplicate(ShotId(9)).has_value());
}

void ShotTests::MoveAndRemoveReorder() {
  HistoryManager history{Project()};
  ShotManager shots(&history);
  const ShotId a = *shots.Add(-1);
  QVERIFY(shots.Add(-1).has_value());
  QVERIFY(shots.Add(-1).has_value());
  QVERIFY(shots.Move(a, 2).has_value());
  QCOMPARE(Names(history.current()),
           QStringList({"Shot 2", "Shot 3", "Shot 1"}));
  QVERIFY(!shots.Move(a, 3).has_value());
  QVERIFY(shots.Remove(a).has_value());
  QCOMPARE(Names(history.current()), QStringList({"Shot 2", "Shot 3"}));
  QVERIFY(!shots.Remove(a).has_value());
}

void ShotTests::SettingsCheckTheirValues() {
  HistoryManager history{Project()};
  ShotManager shots(&history);
  const ShotId id = *shots.Add(-1);
  QVERIFY(!shots.SetLength(id, Frame(0)).has_value());
  QVERIFY(shots.SetLength(id, Frame(12)).has_value());
  QVERIFY(!shots.Rename(id, " ").has_value());
  QVERIFY(shots.Rename(id, " Chorus ").has_value());
  QVERIFY(!shots.SetBackground(id, QColor()).has_value());
  QVERIFY(shots.SetBackground(id, Qt::black).has_value());
  const Shot& shot = *history.current().shots[0];
  QCOMPARE(shot.length, Frame(12));
  QCOMPARE(shot.name, QString("Chorus"));
  QCOMPARE(shot.background, QColor(Qt::black));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ShotTests)
#include "shot_tests.moc"
