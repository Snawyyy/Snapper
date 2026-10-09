#ifndef SNAPPER_TESTS_UI_POSE_STAGE_H_
#define SNAPPER_TESTS_UI_POSE_STAGE_H_

#include <QDir>
#include <QImage>
#include <QTest>

#include <cassert>
#include <memory>
#include <variant>

#include "anim/doll_pose.h"
#include "anim/sampler.h"
#include "bench.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace posestage {

inline const StageFrame kFrame{ShotId(1), Frame(0), 1.0, QPointF(10, 10)};

// A 100x100 project with a 20x20 red doll in the middle, its drawing in
// the bench's folder.
inline void Stage(Bench* bench) {
  assert(bench != nullptr);
  QImage red(20, 20, QImage::Format_ARGB32);
  red.fill(Qt::red);
  QVERIFY(red.save(QDir(bench->dir.path()).filePath("body.png")));
  Doll doll;
  doll.folder = bench->dir.path();
  doll.art.pieces = {{"body", {"body.png"}, 0, {-10, -10}, {20, 20}}};
  doll.rig.pieces = {{"body", "", {10, 10}, 0, -1, {}}};
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{"Dot", {}, false};
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.canvas = {100, 100};
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
}

// The body's pose at frame 0; rest when it has no keys.
inline PiecePose Body(const Bench& bench) {
  assert(!bench.dir.path().isEmpty());
  assert(!bench.history.current().shots.empty());
  const PoseMap poses = SamplePoses(
      std::get<DollLayer>(bench.history.current().shots[0]->layers[0].content),
      Frame(0));
  const auto found = poses.find("body");
  return found == poses.end() ? PiecePose() : found->second;
}

}  // namespace posestage
}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_POSE_STAGE_H_
