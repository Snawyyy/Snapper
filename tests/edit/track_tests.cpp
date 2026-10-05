#include <QTest>

#include <type_traits>

#include "edit/project_edits.h"

namespace snapper {
namespace {

Project Stage() {
  Shot shot;
  shot.id = ShotId(1);
  Layer doll;
  doll.id = LayerId(1);
  doll.content = DollLayer{"Bob", {}, false};
  Layer effect;
  effect.id = LayerId(2);
  effect.content = EffectLayer{};
  shot.layers = {doll, effect};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  return project;
}

// Writes a key at frame 3 with whatever value type the track holds.
Result<Project> KeyAt3(const Project& project, const TrackRef& track) {
  return WithTrack(project, track, [](auto* channel) {
    using Value = typename std::remove_pointer_t<
        decltype(channel)>::value_type;
    SetKey(channel, {Frame(3), Value{}, Ease::kLinear});
    return Result<void>();
  });
}

int KeyCount(const Project& project, const TrackRef& track) {
  int count = -1;
  ReadTrack(*project.shots[0], track, [&count](const auto& channel) {
    count = static_cast<int>(channel.keys.size());
  });
  return count;
}

}  // namespace

class TrackTests final : public QObject {
  Q_OBJECT

 private slots:
  void EveryKindOfTrackTakesKeys();
  void WrongKindsAndGoneLayersFail();
};

void TrackTests::EveryKindOfTrackTakesKeys() {
  const ShotId shot(1);
  const std::vector<TrackRef> tracks = {
      {shot, TrackKind::kLayer, LayerId(1), {}},
      {shot, TrackKind::kPiece, LayerId(1), "arm"},
      {shot, TrackKind::kCamera, {}, {}},
      {shot, TrackKind::kEffectAmount, LayerId(2), {}}};
  for (const TrackRef& track : tracks) {
    const Project before = Stage();
    QCOMPARE(KeyCount(before, track), 0);
    const auto after = KeyAt3(before, track);
    QVERIFY(after.has_value());
    QCOMPARE(KeyCount(*after, track), 1);
    QCOMPARE(KeyCount(before, track), 0);
  }
}

void TrackTests::WrongKindsAndGoneLayersFail() {
  const ShotId shot(1);
  const Project project = Stage();
  QVERIFY(!KeyAt3(project, {shot, TrackKind::kPiece, LayerId(2), "arm"})
               .has_value());
  QVERIFY(!KeyAt3(project, {shot, TrackKind::kEffectAmount, LayerId(1), {}})
               .has_value());
  QVERIFY(!KeyAt3(project, {shot, TrackKind::kLayer, LayerId(9), {}})
               .has_value());
  QVERIFY(!KeyAt3(project, {ShotId(4), TrackKind::kCamera, {}, {}})
               .has_value());
  QCOMPARE(KeyCount(project, {shot, TrackKind::kLayer, LayerId(9), {}}), -1);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::TrackTests)
#include "track_tests.moc"
