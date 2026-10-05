#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include "edit/history_manager.h"
#include "edit/stage_manager.h"

namespace snapper {
namespace {

Project OneShot() {
  Project project;
  Shot shot;
  shot.id = ShotId(1);
  project.shots = {std::make_shared<const Shot>(shot)};
  project.next_shot_id = 2;
  project.dolls["Bob"] = std::make_shared<const Doll>();
  return project;
}

const Shot& TheShot(const HistoryManager& history) {
  return *history.current().shots[0];
}

}  // namespace

class StageTests final : public QObject {
  Q_OBJECT

 private slots:
  void AddsEveryKindOnTop();
  void RefusesWhatCantBeShown();
  void DuplicateMoveAndRemove();
  void SettingsFitTheirKind();
  void ManyLayersChangeTogether();
};

void StageTests::AddsEveryKindOnTop() {
  QTemporaryDir dir;
  const QString picture = QDir(dir.path()).filePath("bg.png");
  QImage image(4, 4, QImage::Format_ARGB32);
  image.fill(Qt::blue);
  QVERIFY(image.save(picture));
  HistoryManager history(OneShot());
  StageManager stage(&history);
  const ShotId shot(1);
  QVERIFY(stage.AddDoll(shot, "Bob").has_value());
  QVERIFY(stage.AddImage(shot, picture).has_value());
  QVERIFY(stage.AddText(shot, "Hello\nworld").has_value());
  const auto effect = stage.AddEffect(shot, EffectKind::kGlitch);
  QVERIFY(effect.has_value());
  const auto& layers = TheShot(history).layers;
  QCOMPARE(layers.size(), size_t{4});
  QCOMPARE(layers[0].name, QString("Bob"));
  QCOMPARE(layers[1].name, QString("bg"));
  QVERIFY(std::holds_alternative<TextLayer>(layers[2].content));
  QCOMPARE(layers[3].id, *effect);
  QCOMPARE(history.UndoLabel(), QString("Add Glitch"));
}

void StageTests::RefusesWhatCantBeShown() {
  HistoryManager history(OneShot());
  StageManager stage(&history);
  QVERIFY(!stage.AddDoll(ShotId(1), "Ann").has_value());
  QVERIFY(!stage.AddImage(ShotId(1), "/no/such.png").has_value());
  QVERIFY(!stage.AddText(ShotId(1), "  ").has_value());
  QVERIFY(!stage.AddEffect(ShotId(5), EffectKind::kFill).has_value());
  QVERIFY(!history.CanUndo());
}

void StageTests::DuplicateMoveAndRemove() {
  HistoryManager history(OneShot());
  StageManager stage(&history);
  const ShotId shot(1);
  const LayerId doll = *stage.AddDoll(shot, "Bob");
  const LayerId text = *stage.AddText(shot, "Hi");
  const auto copy = stage.Duplicate(shot, doll);
  QVERIFY(copy.has_value());
  QCOMPARE(TheShot(history).layers[1].id, *copy);
  QVERIFY(stage.Move(shot, text, 0).has_value());
  QCOMPARE(TheShot(history).layers[0].id, text);
  QVERIFY(!stage.Move(shot, text, 3).has_value());
  QVERIFY(stage.Remove(shot, doll).has_value());
  QVERIFY(!stage.Remove(shot, doll).has_value());
  QCOMPARE(TheShot(history).layers.size(), size_t{2});
}

void StageTests::SettingsFitTheirKind() {
  HistoryManager history(OneShot());
  StageManager stage(&history);
  const ShotId shot(1);
  const LayerId doll = *stage.AddDoll(shot, "Bob");
  const LayerId text = *stage.AddText(shot, "Hi");
  QVERIFY(stage.SetFlipped(shot, doll, true).has_value());
  QVERIFY(!stage.SetFlipped(shot, text, true).has_value());
  TextLayer style;
  style.text = "Bye";
  style.size = 50.0;
  QVERIFY(stage.SetText(shot, text, style).has_value());
  QVERIFY(!stage.SetText(shot, doll, style).has_value());
  style.size = 0.0;
  QVERIFY(!stage.SetText(shot, text, style).has_value());
  QVERIFY(!stage.SetEffect(shot, text, EffectKind::kFill, Qt::red)
               .has_value());
  QVERIFY(stage.SetRange(shot, doll, Frame(2), Frame(5)).has_value());
  QVERIFY(stage.SetVisible(shot, doll, false).has_value());
  QVERIFY(stage.Rename(shot, doll, "Hero").has_value());
  const Layer& layer = TheShot(history).layers[0];
  QCOMPARE(layer.name, QString("Hero"));
  QVERIFY(!layer.is_visible);
  QCOMPARE(layer.start, Frame(2));
  QVERIFY(std::get<DollLayer>(layer.content).is_flipped);
  QCOMPARE(std::get<TextLayer>(TheShot(history).layers[1].content).text,
           QString("Bye"));
}

void StageTests::ManyLayersChangeTogether() {
  HistoryManager history(OneShot());
  StageManager stage(&history);
  const ShotId shot(1);
  const LayerId a = *stage.AddText(shot, "A");
  const LayerId b = *stage.AddText(shot, "B");
  const LayerId c = *stage.AddText(shot, "C");
  QVERIFY(stage.SetRange(shot, a, Frame(10), Frame(5)).has_value());
  QVERIFY(stage.ShiftTiming(shot, {a, b}, 4, 2).has_value());
  const auto& layers = [&history]() -> const std::vector<Layer>& {
    return history.current().shots[0]->layers;
  };
  QCOMPARE(layers()[0].start, Frame(14));
  QCOMPARE(layers()[1].start, Frame(4));
  QCOMPARE(layers()[1].length, Frame(2));
  QVERIFY(stage.ShiftText(shot, {a, c}, 4.0, -100.0).has_value());
  QCOMPARE(std::get<TextLayer>(layers()[2].content).size, 100.0);
  QCOMPARE(std::get<TextLayer>(layers()[2].content).outline_width, 0.0);
  QVERIFY(stage.ShowAll(shot, {a, b}, false).has_value());
  QVERIFY(!layers()[0].is_visible && !layers()[1].is_visible);
  QVERIFY(stage.Restack(shot, {a, b}, 1).has_value());
  QCOMPARE(layers()[0].id, c);
  QCOMPARE(layers()[1].id, a);
  QVERIFY(stage.RemoveAll(shot, {a, c}).has_value());
  QCOMPARE(layers().size(), size_t{1});
  QVERIFY(!stage.ShowAll(shot, {LayerId(99)}, true).has_value());
  const auto copies = stage.DuplicateAll(shot, {b});
  QVERIFY(copies.has_value());
  QCOMPARE(layers().size(), size_t{2});
  QCOMPARE(history.UndoLabel(), QString("Duplicate layers"));
}

}  // namespace snapper

QTEST_MAIN(snapper::StageTests)
#include "stage_tests.moc"
