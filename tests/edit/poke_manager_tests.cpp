#include <QSignalSpy>
#include <QTest>

#include <cassert>
#include <cmath>
#include <memory>

#include "anim/poke_layer.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/poke_manager.h"

namespace snapper {
namespace {

const ShotId kShot(1);
const LayerId kChest(1);
const LayerId kBox(2);
const LayerId kPlain(3);

// A jelly chest, a picture and a doll with no springs on one shot.
Project Stage() {
  Doll chest;
  chest.name = "Chest";
  chest.art.pieces = {{"chest", {"c.png"}, 0, {0, 0}, {20, 20}}};
  chest.rig.pieces = {{"chest", "", {10, 0}, 0, -1, {2, 2}}};
  for (int point = 3; point < 9; ++point) {
    DragNode node{point};
    node.kind = NodeKind::kJelly;
    chest.rig.pieces[0].drag_nodes.push_back(node);
  }
  Doll plain = chest;
  plain.name = "Plain";
  plain.rig.pieces[0].drag_nodes.clear();
  Project project;
  project.dolls["Chest"] = std::make_shared<const Doll>(chest);
  project.dolls["Plain"] = std::make_shared<const Doll>(plain);
  Shot shot;
  shot.id = kShot;
  shot.length = Frame(400);
  const QStringList docs = {"Chest", "", "Plain"};
  for (int i = 0; i < 3; ++i) {
    Layer layer;
    layer.id = LayerId(i + 1);
    layer.content = i == 1 ? LayerContent(ImageLayer{"/x.png"})
                           : LayerContent(DollLayer{docs[i], {}, false});
    shot.layers.push_back(layer);
  }
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots[0]->layers.size() == 3);
  assert(FindDoll(project, "Chest") != nullptr);
  return project;
}

// Ticks until the poke ends or the cap; returns the ticks run.
int TickToTheEnd(PokeManager* poke) {
  assert(poke != nullptr);
  assert(kMaxPokeFrames > 0);
  int ticks = 0;
  while (poke->IsActive() && ticks <= kMaxPokeFrames + 1) {
    poke->Tick();
    ++ticks;
  }
  return ticks;
}

double Wobble(const PokeManager& poke) {
  assert(poke.poses().size() <= 16);
  assert(kMaxPokeFrames > 0);
  const auto found = poke.poses().find("chest");
  double most = 0.0;
  const bool has_warp = found != poke.poses().end();
  for (size_t i = 0; has_warp && i < found->second.warp.size(); ++i) {
    most = std::max(most, std::hypot(found->second.warp[i].x(),
                                     found->second.warp[i].y()));
  }
  return most;
}

}  // namespace

class PokeManagerTests final : public QObject {
  Q_OBJECT

 private slots:
  void SaysWhyAThingCantBePoked();
  void AShovedDollWobblesThenSettlesAndEnds();
  void NothingIsKeyedAndHistoryIsUntouched();
  void EndStopsAtOnce();
  void MovingThePlayheadEnds();
  void AnEditRunsThePokeAgain();
  void LettingGoWithoutAShoveSettlesFast();

 private:
  // A poke on the chest at frame 80 with the mouse shoved 100 px.
  static void Shove(PokeManager* poke);
};

void PokeManagerTests::Shove(PokeManager* poke) {
  assert(poke != nullptr);
  const QString why = poke->Begin(kShot, kChest, Frame(80), QPointF());
  QVERIFY(why.isEmpty());
  poke->Move(QPointF(100, 0));
}

void PokeManagerTests::SaysWhyAThingCantBePoked() {
  HistoryManager history{Stage()};
  PlaybackManager playback{&history};
  PokeManager poke{&history, &playback};
  QVERIFY(poke.WhyNot(kShot, kChest).isEmpty());
  QCOMPARE(poke.WhyNot(kShot, LayerId()), QString("Hover a doll to poke it."));
  QCOMPARE(poke.WhyNot(kShot, kBox), QString("Hover a doll to poke it."));
  QVERIFY(poke.WhyNot(kShot, kPlain).contains("no drag"));
  QVERIFY(!poke.Begin(kShot, LayerId(), Frame(3), QPointF()).isEmpty());
  QVERIFY(!poke.IsActive());
}

void PokeManagerTests::AShovedDollWobblesThenSettlesAndEnds() {
  HistoryManager history{Stage()};
  PlaybackManager playback{&history};
  PokeManager poke{&history, &playback};
  QSignalSpy changed(&poke, &PokeManager::Changed);
  Shove(&poke);
  QVERIFY(poke.IsActive() && poke.IsHeld());
  for (int i = 0; i < 6; ++i) {
    poke.Tick();
  }
  QVERIFY2(Wobble(poke) > 0.5, qPrintable(QString::number(Wobble(poke))));
  QVERIFY(poke.IsActive());
  poke.Release();
  QVERIFY(!poke.IsHeld());
  const int ticks = TickToTheEnd(&poke);
  QVERIFY(!poke.IsActive());
  QVERIFY2(ticks < kMaxPokeFrames, qPrintable(QString::number(ticks)));
  QVERIFY(poke.poses().empty());
  QVERIFY(changed.count() > 6);
}

void PokeManagerTests::NothingIsKeyedAndHistoryIsUntouched() {
  HistoryManager history{Stage()};
  PlaybackManager playback{&history};
  PokeManager poke{&history, &playback};
  const Project before = history.current();
  const int revision = history.revision();
  QSignalSpy history_changed(&history, &HistoryManager::Changed);
  Shove(&poke);
  for (int i = 0; i < 10; ++i) {
    poke.Tick();
  }
  poke.Release();
  TickToTheEnd(&poke);
  QVERIFY(history.current() == before);
  QCOMPARE(history.revision(), revision);
  QVERIFY(!history.CanUndo());
  QCOMPARE(history_changed.count(), 0);
}

void PokeManagerTests::EndStopsAtOnce() {
  HistoryManager history{Stage()};
  PlaybackManager playback{&history};
  PokeManager poke{&history, &playback};
  Shove(&poke);
  poke.Tick();
  poke.End();
  QVERIFY(!poke.IsActive());
  QVERIFY(poke.poses().empty());
  QCOMPARE(poke.ticks(), 0);
  poke.Tick();
  QVERIFY(poke.poses().empty());
}

void PokeManagerTests::MovingThePlayheadEnds() {
  HistoryManager history{Stage()};
  PlaybackManager playback{&history};
  PokeManager poke{&history, &playback};
  Shove(&poke);
  QVERIFY(poke.IsActive());
  playback.Seek(Frame(120));
  QVERIFY(!poke.IsActive());
}

void PokeManagerTests::AnEditRunsThePokeAgain() {
  HistoryManager history{Stage()};
  PlaybackManager playback{&history};
  PokeManager poke{&history, &playback};
  Shove(&poke);
  for (int i = 0; i < 4; ++i) {
    poke.Tick();
  }
  const double before = Wobble(poke);
  QSignalSpy changed(&poke, &PokeManager::Changed);
  // A firmer jelly: the same poke, run again on the edited doll. (A
  // softer one would swing as far as a jelly may and show no change.)
  Project next = history.current();
  Doll doll = *next.dolls["Chest"];
  for (DragNode& node : doll.rig.pieces[0].drag_nodes) {
    node.spring = 1.0;
    node.inertia = 0.0;
  }
  next.dolls["Chest"] = std::make_shared<const Doll>(doll);
  history.Commit("Firm up", next);
  QVERIFY(poke.IsActive());
  QCOMPARE(poke.ticks(), 5);
  QVERIFY(changed.count() >= 1);
  QVERIFY(std::abs(Wobble(poke) - before) > 1e-6);
  // Deleting the doll ends the poke.
  Project gone = history.current();
  gone.shots.clear();
  history.Commit("Clear", gone);
  QVERIFY(!poke.IsActive());
}

void PokeManagerTests::LettingGoWithoutAShoveSettlesFast() {
  HistoryManager history{Stage()};
  PlaybackManager playback{&history};
  PokeManager poke{&history, &playback};
  const QString why = poke.Begin(kShot, kChest, Frame(80), QPointF());
  QVERIFY(why.isEmpty());
  poke.Release();
  const int ticks = TickToTheEnd(&poke);
  QVERIFY2(ticks <= 6, qPrintable(QString::number(ticks)));
  QVERIFY(!poke.IsActive());
}

}  // namespace snapper

QTEST_MAIN(snapper::PokeManagerTests)
#include "poke_manager_tests.moc"
