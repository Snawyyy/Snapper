#include <QSignalSpy>
#include <QTest>

#include "edit/history_manager.h"
#include "edit/selection_manager.h"

namespace snapper {
namespace {

Project Stage() {
  Doll doll;
  doll.rig.pieces = {{"head", "", {}, 0, -1, {}}, {"arm", "", {}, 0, -1, {}}};
  Shot shot;
  shot.id = ShotId(1);
  Layer layer;
  layer.id = LayerId(1);
  DollLayer posed{"Bob", {}, false};
  SetKey(&posed.pieces["head"], {Frame(4), PiecePose(), Ease::kStep});
  layer.content = posed;
  shot.layers = {layer};
  Shot other;
  other.id = ShotId(2);
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot),
                   std::make_shared<const Shot>(other)};
  return project;
}

const KeyRef kHeadKey{{ShotId(1), TrackKind::kPiece, LayerId(1), "head"},
                      Frame(4)};

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
  selection.SelectPieces({"head", "tail"}, false);
  QCOMPARE(selection.pieces(), std::set<QString>({"head"}));
  selection.SelectPieces({"arm"}, true);
  QCOMPARE(selection.pieces().size(), size_t{2});
  KeyRef missing = kHeadKey;
  missing.frame = Frame(5);
  selection.SelectKeys({kHeadKey, missing}, false);
  QCOMPARE(selection.keys(), std::set<KeyRef>({kHeadKey}));
  selection.SelectLayer(LayerId(9));
  QCOMPARE(selection.layer(), LayerId(1));
  QCOMPARE(changed.count(), 4);
}

void SelectionTests::PickingAnotherShotDropsTheRest() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  selection.SelectLayer(LayerId(1));
  selection.SelectPieces({"head"}, false);
  selection.SelectKeys({kHeadKey}, false);
  selection.SelectShot(ShotId(2));
  QCOMPARE(selection.shot(), ShotId(2));
  QVERIFY(selection.pieces().empty());
  QVERIFY(selection.keys().empty());
  selection.SelectShot(ShotId(7));
  QCOMPARE(selection.shot(), ShotId(2));
}

void SelectionTests::EditsDropWhatIsGone() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  selection.SelectLayer(LayerId(1));
  selection.SelectPieces({"head", "arm"}, false);
  selection.SelectKeys({kHeadKey}, false);
  Project next = history.current();
  Doll doll = *next.dolls.at("Bob");
  doll.rig.pieces.pop_back();
  next.dolls["Bob"] = std::make_shared<const Doll>(doll);
  Shot shot = *next.shots[0];
  std::get<DollLayer>(shot.layers[0].content).pieces.clear();
  next.shots[0] = std::make_shared<const Shot>(shot);
  history.Commit("Edit", next);
  QCOMPARE(selection.pieces(), std::set<QString>({"head"}));
  QVERIFY(selection.keys().empty());
  next.shots.erase(next.shots.begin());
  history.Commit("Remove", next);
  QCOMPARE(selection.shot(), ShotId(2));
  QVERIFY(!selection.layer().IsValid());
  QVERIFY(selection.pieces().empty());
}

void SelectionTests::ManyThingsCombineByMode() {
  HistoryManager history(Stage());
  SelectionManager selection(&history);
  const Pick head{LayerId(1), "head"};
  const Pick arm{LayerId(1), "arm"};
  selection.PickThings({head}, PickMode::kReplace, LayerId(1));
  selection.PickThings({arm}, PickMode::kAdd, LayerId(1));
  QCOMPARE(selection.picks(), std::set<Pick>({head, arm}));
  selection.PickThings({head}, PickMode::kToggle, LayerId(1));
  QCOMPARE(selection.picks(), std::set<Pick>({arm}));
  selection.PickThings({arm, {LayerId(1), "tail"}}, PickMode::kRemove,
                       LayerId(1));
  QVERIFY(selection.picks().empty());
  QVERIFY(!selection.layer().IsValid());
  selection.PickThings({head, {LayerId(1), {}}}, PickMode::kAdd, LayerId(1));
  // A layer picked by a piece isn't also picked whole.
  QCOMPARE(selection.picks(), std::set<Pick>({head}));
  const auto tracks = selection.PickedTracks();
  QCOMPARE(tracks.size(), size_t{1});
  QCOMPARE(tracks[0].kind, TrackKind::kPiece);
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
