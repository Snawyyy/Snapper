#include <QTest>

#include <cassert>
#include <memory>
#include <utility>
#include <vector>

#include "edit/change_diff.h"

namespace snapper {
namespace {

const ShotId kShot(1);
const LayerId kBob(1);
const LayerId kAmy(2);

struct Keyed final {
  int frame;
  double angle;
};

Layer DollWith(LayerId id, const std::vector<Keyed>& keys) {
  assert(id.IsValid());
  assert(keys.size() < 100);
  Layer layer;
  layer.id = id;
  layer.name = "Doll";
  layer.content = DollLayer{"Doll", {}, false};
  for (const Keyed& key : keys) {
    PiecePose pose;
    pose.rotation = key.angle;
    SetKey(&layer.transform, Key<PiecePose>{Frame(key.frame), pose});
  }
  return layer;
}

Project StageOf(std::vector<Layer> layers) {
  assert(layers.size() < 100);
  assert(kShot.IsValid());
  Shot shot;
  shot.id = kShot;
  shot.layers = std::move(layers);
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

Project Bob(const std::vector<Keyed>& keys) {
  assert(keys.size() < 100);
  assert(kBob.IsValid());
  return StageOf({DollWith(kBob, keys)});
}

Clip ClipOn(int id, int start, int length) {
  assert(id > 0);
  assert(length > 0);
  Clip clip;
  clip.id = ClipId(id);
  clip.source = VideoSource{"a.mp4", Frame(100)};
  clip.start = Frame(start);
  clip.length = Frame(length);
  return clip;
}

Project ReelOf(const std::vector<std::vector<Clip>>& tracks) {
  assert(tracks.size() <= static_cast<size_t>(kMaxReelTracks));
  assert(!tracks.empty());
  Project project;
  project.reel.tracks.clear();
  for (const auto& clips : tracks) {
    project.reel.tracks.push_back({clips});
  }
  return project;
}

}  // namespace

class ChangeDiffTests final : public QObject {
  Q_OBJECT

 private slots:
  void NothingChangedReportsNothing();
  void ANewKeyIsAdded();
  void ADeletedKeyIsRemoved();
  void ASlidKeyIsOneMove();
  void ARewrittenKeyIsNotAMove();
  void ManyKeysMoveTogether();
  void AMoveOntoAnotherKeysFrameIsNotAMove();
  void KeysAreReportedOnTheirOwnRow();
  void AMissingShotReportsNothing();
  void ANewLayerCountsItsKeysAsAdded();
  void AGoneLayerReportsNothing();
  void CameraKeysUseTheInvalidLayer();
  void ClipsAreFoundById();
  void AClipMovedToAnotherTrackIsMoved();
  void AClipTrimmedAtItsEndIsNotMoved();
};

void ChangeDiffTests::NothingChangedReportsNothing() {
  const Project same = Bob({{4, 10}, {8, 20}});
  QVERIFY(DiffKeys(same, same, kShot).IsEmpty());
  QVERIFY(DiffClips(same, same).IsEmpty());
}

void ChangeDiffTests::ANewKeyIsAdded() {
  const KeyChanges got =
      DiffKeys(Bob({{4, 10}}), Bob({{4, 10}, {8, 20}}), kShot);
  QCOMPARE(got.added, std::vector<KeyChange>({{kBob, Frame(8)}}));
  QVERIFY(got.removed.empty());
  QVERIFY(got.moved.empty());
}

void ChangeDiffTests::ADeletedKeyIsRemoved() {
  const KeyChanges got =
      DiffKeys(Bob({{4, 10}, {8, 20}}), Bob({{4, 10}}), kShot);
  QCOMPARE(got.removed, std::vector<KeyChange>({{kBob, Frame(8)}}));
  QVERIFY(got.added.empty());
  QVERIFY(got.moved.empty());
}

void ChangeDiffTests::ASlidKeyIsOneMove() {
  const KeyChanges got = DiffKeys(Bob({{4, 10}}), Bob({{9, 10}}), kShot);
  QCOMPARE(got.moved, std::vector<KeyMove>({{kBob, Frame(4), Frame(9)}}));
  QVERIFY(got.added.empty());
  QVERIFY(got.removed.empty());
}

void ChangeDiffTests::ARewrittenKeyIsNotAMove() {
  // Same frame, new value: the key stayed, so nothing came or went.
  QVERIFY(DiffKeys(Bob({{4, 10}}), Bob({{4, 99}}), kShot).IsEmpty());
  // Different frame and different pose: one key went, one came.
  const KeyChanges got = DiffKeys(Bob({{4, 10}}), Bob({{9, 99}}), kShot);
  QCOMPARE(got.removed, std::vector<KeyChange>({{kBob, Frame(4)}}));
  QCOMPARE(got.added, std::vector<KeyChange>({{kBob, Frame(9)}}));
  QVERIFY(got.moved.empty());
}

void ChangeDiffTests::ManyKeysMoveTogether() {
  const KeyChanges got = DiffKeys(Bob({{2, 5}, {4, 10}, {6, 15}}),
                                  Bob({{3, 5}, {5, 10}, {7, 15}}), kShot);
  QCOMPARE(got.moved.size(), size_t{3});
  QCOMPARE(got.moved[0], (KeyMove{kBob, Frame(2), Frame(3)}));
  QCOMPARE(got.moved[2], (KeyMove{kBob, Frame(6), Frame(7)}));
  QVERIFY(got.added.empty() && got.removed.empty());
}

void ChangeDiffTests::AMoveOntoAnotherKeysFrameIsNotAMove() {
  // The key on 4 slid onto 8 and swallowed the one there.
  const KeyChanges got =
      DiffKeys(Bob({{4, 10}, {8, 20}}), Bob({{8, 10}}), kShot);
  QVERIFY(got.moved.empty());
  QCOMPARE(got.removed, std::vector<KeyChange>({{kBob, Frame(4)}}));
  QVERIFY(got.added.empty());
}

void ChangeDiffTests::KeysAreReportedOnTheirOwnRow() {
  const Project before =
      StageOf({DollWith(kBob, {{4, 10}}), DollWith(kAmy, {{4, 10}})});
  const Project after =
      StageOf({DollWith(kBob, {{4, 10}}), DollWith(kAmy, {{4, 10}, {6, 3}})});
  const KeyChanges got = DiffKeys(before, after, kShot);
  QCOMPARE(got.added, std::vector<KeyChange>({{kAmy, Frame(6)}}));
  QVERIFY(got.removed.empty() && got.moved.empty());
}

void ChangeDiffTests::AMissingShotReportsNothing() {
  QVERIFY(DiffKeys(Bob({{4, 1}}), Bob({{5, 1}}), ShotId(9)).IsEmpty());
  QVERIFY(DiffKeys(Project(), Bob({{5, 1}}), kShot).IsEmpty());
  QVERIFY(DiffKeys(Bob({{5, 1}}), Project(), kShot).IsEmpty());
}

void ChangeDiffTests::ANewLayerCountsItsKeysAsAdded() {
  const Project after =
      StageOf({DollWith(kBob, {{4, 10}}), DollWith(kAmy, {{2, 1}, {5, 2}})});
  const KeyChanges got = DiffKeys(Bob({{4, 10}}), after, kShot);
  QCOMPARE(got.added,
           std::vector<KeyChange>({{kAmy, Frame(2)}, {kAmy, Frame(5)}}));
  QVERIFY(got.removed.empty() && got.moved.empty());
}

void ChangeDiffTests::AGoneLayerReportsNothing() {
  const Project before =
      StageOf({DollWith(kBob, {{4, 10}}), DollWith(kAmy, {{2, 1}})});
  QVERIFY(DiffKeys(before, Bob({{4, 10}}), kShot).IsEmpty());
  QVERIFY(DiffClips(before, Bob({{4, 10}})).IsEmpty());
}

void ChangeDiffTests::CameraKeysUseTheInvalidLayer() {
  Project after = Bob({});
  Shot shot = *after.shots.front();
  SetKey(&shot.camera, Key<CameraPose>{Frame(3), CameraPose{}});
  after.shots = {std::make_shared<const Shot>(shot)};
  const KeyChanges got = DiffKeys(Bob({}), after, kShot);
  QCOMPARE(got.added, std::vector<KeyChange>({{LayerId(), Frame(3)}}));
  QVERIFY(got.removed.empty() && got.moved.empty());
}

void ChangeDiffTests::ClipsAreFoundById() {
  const Project before = ReelOf({{ClipOn(1, 0, 10), ClipOn(2, 20, 5)}, {}});
  const Project after = ReelOf({{ClipOn(1, 0, 10), ClipOn(3, 12, 4)}, {}});
  const ClipChanges got = DiffClips(before, after);
  QCOMPARE(got.added,
           std::vector<ClipChange>({{ClipId(3), 0, Frame(12), Frame(4)}}));
  QCOMPARE(got.removed,
           std::vector<ClipChange>({{ClipId(2), 0, Frame(20), Frame(5)}}));
  QVERIFY(got.moved.empty());
}

void ChangeDiffTests::AClipMovedToAnotherTrackIsMoved() {
  const Project before = ReelOf({{ClipOn(1, 0, 10)}, {}});
  const Project after = ReelOf({{}, {ClipOn(1, 0, 10)}});
  const ClipChanges got = DiffClips(before, after);
  QCOMPARE(got.moved, std::vector<ClipChange>({{ClipId(1), 1, Frame(0),
                                                Frame(10), 0, Frame(0)}}));
  const Project slid = ReelOf({{ClipOn(1, 5, 10)}, {}});
  QCOMPARE(DiffClips(before, slid).moved.size(), size_t{1});
}

void ChangeDiffTests::AClipTrimmedAtItsEndIsNotMoved() {
  const Project before = ReelOf({{ClipOn(1, 0, 10)}, {}});
  const Project after = ReelOf({{ClipOn(1, 0, 6)}, {}});
  QVERIFY(DiffClips(before, after).IsEmpty());
  QVERIFY(DiffKeys(before, after, kShot).IsEmpty());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ChangeDiffTests)
#include "change_diff_tests.moc"
