#include <QSignalSpy>
#include <QTest>

#include <cassert>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"

namespace snapper {
namespace {

// Shot 1 holds Bob (keyed at 4) and Amy; shot 2 is empty.
Project Stage() {
  Shot shot;
  shot.id = ShotId(1);
  Layer bob;
  bob.id = LayerId(1);
  bob.content = CharacterLayer{"Bob", {}, false};
  SetKey(&bob.transform, {Frame(4), LayerPose(), Ease::kStep});
  Layer amy;
  amy.id = LayerId(2);
  amy.content = CharacterLayer{"Amy", {}, false};
  shot.layers = {bob, amy};
  Shot other;
  other.id = ShotId(2);
  Project project;
  project.shots = {std::make_shared<const Shot>(shot),
                   std::make_shared<const Shot>(other)};
  assert(project.shots.size() == 2);
  return project;
}

const KeyRef kBobKey{{ShotId(1), TrackKind::kLayer, LayerId(1)}, Frame(4)};

}  // namespace

class SelectionTests final : public QObject {
  Q_OBJECT

 private slots:
  void StartsOnTheFirstShot();
  void PicksOnlyWhatExists();
  void PickingAnotherShotDropsTheRest();
  void EditsDropWhatIsGone();
  void ManyThingsCombineByMode();
  void ManyShotsKeepAFocus();
};

void SelectionTests::StartsOnTheFirstShot() {
  HistoryManager history(Stage());
  const SelectionManager selection(&history);
  QCOMPARE(selection.shot(), ShotId(1));
  QVERIFY(!selection.layer().IsValid());
}

void SelectionTests::PicksOnlyWhatExists() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  QSignalSpy changed(&selection, &SelectionManager::Changed);
  selection.SelectLayer(LayerId(1));
  KeyRef missing = kBobKey;
  missing.frame = Frame(5);
  selection.SelectKeys({kBobKey, missing}, false);
  QCOMPARE(selection.keys(), std::set<KeyRef>({kBobKey}));
  selection.SelectLayer(LayerId(9));
  QCOMPARE(selection.layer(), LayerId(1));
  QCOMPARE(changed.count(), 2);
}

void SelectionTests::PickingAnotherShotDropsTheRest() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  selection.SelectLayer(LayerId(1));
  selection.SelectKeys({kBobKey}, false);
  selection.SelectShot(ShotId(2));
  QCOMPARE(selection.shot(), ShotId(2));
  QVERIFY(selection.picks().empty());
  QVERIFY(selection.keys().empty());
  selection.SelectShot(ShotId(7));
  QCOMPARE(selection.shot(), ShotId(2));
}

void SelectionTests::EditsDropWhatIsGone() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  selection.PickThings({Pick{LayerId(1)}, Pick{LayerId(2)}},
                       PickMode::kReplace, LayerId(1));
  selection.SelectKeys({kBobKey}, false);
  Project next = history.current();
  Shot shot = *next.shots[0];
  shot.layers[0].transform.keys.clear();
  shot.layers.pop_back();
  next.shots[0] = std::make_shared<const Shot>(shot);
  history.Commit("Edit", next);
  QCOMPARE(selection.picks(), std::set<Pick>({Pick{LayerId(1)}}));
  QVERIFY(selection.keys().empty());
  next.shots.erase(next.shots.begin());
  history.Commit("Remove", next);
  QCOMPARE(selection.shot(), ShotId(2));
  QVERIFY(!selection.layer().IsValid());
  QVERIFY(selection.picks().empty());
}

void SelectionTests::ManyThingsCombineByMode() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  const Pick bob{LayerId(1)};
  const Pick amy{LayerId(2)};
  selection.PickThings({bob}, PickMode::kReplace, LayerId(1));
  selection.PickThings({amy}, PickMode::kAdd, LayerId(2));
  QCOMPARE(selection.picks(), std::set<Pick>({bob, amy}));
  selection.PickThings({bob}, PickMode::kToggle, LayerId(2));
  QCOMPARE(selection.picks(), std::set<Pick>({amy}));
  selection.PickThings({amy, Pick{LayerId(9)}}, PickMode::kRemove,
                       LayerId(2));
  QVERIFY(selection.picks().empty());
  QVERIFY(!selection.layer().IsValid());
  // What is not on the shot is never picked.
  selection.PickThings({bob, Pick{LayerId(9)}}, PickMode::kAdd, LayerId(1));
  QCOMPARE(selection.picks(), std::set<Pick>({bob}));
  const auto tracks = selection.PickedTracks();
  QCOMPARE(tracks.size(), size_t{1});
  QCOMPARE(tracks[0].kind, TrackKind::kLayer);
  QCOMPARE(selection.PickedLayers(), std::vector<LayerId>({LayerId(1)}));
}

void SelectionTests::ManyShotsKeepAFocus() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  selection.PickShots({ShotId(2)}, PickMode::kAdd);
  QCOMPARE(selection.shots(), std::set<ShotId>({ShotId(1), ShotId(2)}));
  QCOMPARE(selection.shot(), ShotId(1));
  selection.PickShots({ShotId(1)}, PickMode::kToggle);
  QCOMPARE(selection.shot(), ShotId(2));
  selection.PickShots({ShotId(2)}, PickMode::kRemove);
  QCOMPARE(selection.shots(), std::set<ShotId>({ShotId(2)}));
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::SelectionTests)
#include "selection_tests.moc"
