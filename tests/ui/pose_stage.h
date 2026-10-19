#ifndef SNAPPER_TESTS_UI_POSE_STAGE_H_
#define SNAPPER_TESTS_UI_POSE_STAGE_H_

#include <QDir>
#include <QImage>
#include <QTest>

#include <cassert>
#include <memory>

#include "anim/sampler.h"
#include "bench.h"
#include "edit/character_manager.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace posestage {

inline const StageFrame kFrame{ShotId(1), Frame(0), 1.0, QPointF(10, 10)};

// A 100x100 project with a 20x20 red picture layer ("Dot") in the
// middle, its file in the bench's folder.
inline void Stage(Bench* bench) {
  assert(bench != nullptr);
  QImage red(20, 20, QImage::Format_ARGB32);
  red.fill(Qt::red);
  const QString path = QDir(bench->dir.path()).filePath("body.png");
  QVERIFY(red.save(path));
  Layer layer;
  layer.id = LayerId(1);
  layer.name = "Dot";
  layer.content = ImageLayer{path};
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.canvas = {100, 100};
  project.next_shot_id = 2;
  project.next_layer_id = 2;
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
}

// Adds Bo, a character with one bone from the frame's middle 30 pixels
// straight down, as layer 2, and picks it. Answers the bone; its handle
// sits at kBoneTip on the widget.
inline const QPointF kBoneTip(60, 90);

inline BoneId StageBone(Bench* bench) {
  assert(bench != nullptr);
  const auto made = bench->characters.Create("Bo");
  assert(made.has_value());
  const Character* bo = FindCharacter(bench->history.current(), "Bo");
  assert(bo != nullptr);
  const auto bone = bench->characters.AddBone("Bo", bo->parts[0].id,
                                              {0, 0}, {0, 30}, BoneId());
  assert(bone.has_value());
  const auto layer = bench->stage.AddCharacter(ShotId(1), "Bo");
  assert(layer.has_value());
  bench->selection.PickThings({Pick{*layer}}, PickMode::kReplace, *layer);
  return *bone;
}

// The Dot layer's pose at frame 0; rest when it has no keys.
inline LayerPose Body(const Bench& bench) {
  assert(!bench.dir.path().isEmpty());
  assert(!bench.history.current().shots.empty());
  return Sample(bench.history.current().shots[0]->layers[0].transform,
                Frame(0), LayerPose());
}

}  // namespace posestage
}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_POSE_STAGE_H_
