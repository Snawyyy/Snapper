#ifndef SNAPPER_TESTS_UI_POSE_STAGE_H_
#define SNAPPER_TESTS_UI_POSE_STAGE_H_

#include <QDir>
#include <QImage>
#include <QTest>

#include <cassert>
#include <memory>

#include "anim/sampler.h"
#include "bench.h"
#include "ui/pose_tool.h"

namespace snapper {
namespace posestage {

inline const StageFrame kFrame{ShotId(1), Frame(0), 1.0, QPointF(10, 10)};

// A 100x100 project with a 20x20 red picture layer ("Dot") in the
// middle, its file in the bench's folder, keyed there at frame 0.
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
  LayerPose middle;
  middle.offset = QPointF(50, 50);
  layer.transform.keys.push_back(Key<LayerPose>{Frame(0), middle});
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.canvas = {100, 100};
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
}

// The Dot layer's pose at frame 0.
inline LayerPose Body(const Bench& bench) {
  assert(!bench.dir.path().isEmpty());
  assert(!bench.history.current().shots.empty());
  return Sample(bench.history.current().shots[0]->layers[0].transform,
                Frame(0), LayerPose());
}

}  // namespace posestage
}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_POSE_STAGE_H_
