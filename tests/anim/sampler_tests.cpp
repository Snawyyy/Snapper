#include <QTest>

#include "anim/master_timeline.h"
#include "anim/mirror.h"
#include "anim/presets.h"
#include "anim/sampler.h"

namespace snapper {
namespace {

std::shared_ptr<const Shot> MakeShot(int id, int length) {
  Shot shot;
  shot.id = ShotId(id);
  shot.length = Frame(length);
  return std::make_shared<const Shot>(shot);
}

}  // namespace

class SamplerTests final : public QObject {
  Q_OBJECT

 private slots:
  void StepKeysHold();
  void EasedKeysBlend();
  void EmptyChannelGivesFallback();
  void DrawingsSwapWithoutBlending();
  void ShotsFollowEachOther();
  void PresetsLoopOnTheirHold();
  void ShakeIsTheSameEveryTime();
  void MirrorNamesSwapSides();
};

void SamplerTests::StepKeysHold() {
  Channel<double> channel;
  SetKey(&channel, {Frame(0), 0.0, Ease::kStep});
  SetKey(&channel, {Frame(10), 10.0, Ease::kStep});
  QCOMPARE(Sample(channel, Frame(9), -1.0), 0.0);
  QCOMPARE(Sample(channel, Frame(10), -1.0), 10.0);
  QCOMPARE(Sample(channel, Frame(500), -1.0), 10.0);
}

void SamplerTests::EasedKeysBlend() {
  Channel<double> channel;
  SetKey(&channel, {Frame(4), 0.0, Ease::kLinear});
  SetKey(&channel, {Frame(8), 8.0, Ease::kStep});
  QCOMPARE(Sample(channel, Frame(0), -1.0), 0.0);
  QCOMPARE(Sample(channel, Frame(6), -1.0), 4.0);
  channel.keys[0].ease = Ease::kEaseIn;
  QCOMPARE(Sample(channel, Frame(6), -1.0), 2.0);
  QCOMPARE(EaseAmount(Ease::kEaseOut, 0.5), 0.75);
  QCOMPARE(EaseAmount(Ease::kEaseInOut, 0.5), 0.5);
}

void SamplerTests::EmptyChannelGivesFallback() {
  const Channel<CameraPose> channel;
  CameraPose fallback;
  fallback.zoom = 2.0;
  QCOMPARE(Sample(channel, Frame(3), fallback).zoom, 2.0);
}

void SamplerTests::DrawingsSwapWithoutBlending() {
  PiecePose a;
  a.drawing = 1;
  a.warp = {QPointF(0, 0), QPointF(4, 0)};
  PiecePose b;
  b.drawing = 2;
  b.rotation = 90.0;
  const PiecePose mid = Lerp(a, b, 0.5);
  QCOMPARE(mid.drawing, 1);
  QCOMPARE(mid.rotation, 45.0);
  QCOMPARE(mid.warp[1], QPointF(2, 0));
}

void SamplerTests::ShotsFollowEachOther() {
  Project project;
  project.shots = {MakeShot(1, 10), MakeShot(2, 20)};
  QCOMPARE(ShotStart(project, 1), Frame(10));
  QCOMPARE(TotalLength(project), Frame(30));
  const ShotMoment moment = Locate(project, Frame(12));
  QCOMPARE(moment.shot, 1);
  QCOMPARE(moment.local, Frame(2));
  QCOMPARE(Locate(project, Frame(30)).shot, -1);
  QCOMPARE(TotalLength(Project()), Frame(0));
}

void SamplerTests::PresetsLoopOnTheirHold() {
  PresetSettings settings{MotionPreset::kBob, 6.0, 3, 12};
  const auto keys = PresetKeys(settings);
  QCOMPARE(keys.size(), size_t{4});
  QCOMPARE(keys[1].frame, Frame(3));
  QCOMPARE(keys[1].value.offset, QPointF(0, 6));
  QCOMPARE(keys[1].ease, Ease::kStep);
  settings.preset = MotionPreset::kSway;
  QCOMPARE(PresetKeys(settings)[0].ease, Ease::kEaseInOut);
  settings.length = 0;
  QVERIFY(PresetKeys(settings).empty());
  QVERIFY(!PresetName(MotionPreset::kNod).isEmpty());
}

void SamplerTests::ShakeIsTheSameEveryTime() {
  const PresetSettings settings{MotionPreset::kShake, 5.0, 1, 8};
  const auto first = PresetKeys(settings);
  QCOMPARE(PresetKeys(settings), first);
  QVERIFY(first[0].value.offset != first[1].value.offset);
  for (const auto& key : first) {
    QVERIFY(std::abs(key.value.offset.x()) <= 5.0);
  }
  PiecePose base;
  base.scale_x = 2.0;
  PiecePose change;
  change.scale_x = 0.5;
  change.rotation = 10.0;
  QCOMPARE(AddPose(base, change).scale_x, 1.0);
}

void SamplerTests::MirrorNamesSwapSides() {
  QCOMPARE(MirrorName("arm_l"), QString("arm_r"));
  QCOMPARE(MirrorName("L_leg"), QString("R_leg"));
  QCOMPARE(MirrorName("Left hand"), QString("Right hand"));
  QCOMPARE(MirrorName("RIGHT_EYE"), QString("LEFT_EYE"));
  QCOMPARE(MirrorName("eye.R"), QString("eye.L"));
  QCOMPARE(MirrorName("upper_l_arm"), QString("upper_r_arm"));
  QCOMPARE(MirrorName("lip"), QString("lip"));
  QCOMPARE(MirrorName("cleft"), QString("cleft"));
  QCOMPARE(SideOf("arm_l"), -1);
  QCOMPARE(SideOf("Right hand"), 1);
  QCOMPARE(SideOf("head"), 0);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::SamplerTests)
#include "sampler_tests.moc"
