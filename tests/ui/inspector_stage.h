#ifndef SNAPPER_TESTS_UI_INSPECTOR_STAGE_H_
#define SNAPPER_TESTS_UI_INSPECTOR_STAGE_H_

#include <cassert>
#include <memory>
#include <variant>

#include "anim/doll_pose.h"
#include "bench.h"

namespace snapper {

// One shot with Bob (a doll with a head, picked) and a text layer.
inline const ShotId kShot(1);

inline void Stage(Bench* bench) {
  assert(bench != nullptr);
  Doll doll;
  doll.rig.pieces = {{"head", "", {}, 0, -1, {}}};
  Layer bob;
  bob.id = LayerId(1);
  bob.name = "Bob";
  bob.content = DollLayer{"Bob", {}, false};
  Layer words;
  words.id = LayerId(2);
  words.name = "Words";
  words.content = TextLayer{};
  std::get<TextLayer>(words.content).text = "Hi";
  Shot shot;
  shot.id = kShot;
  shot.layers = {bob, words};
  Project project;
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  bench->selection.SelectLayer(LayerId(1));
  bench->selection.SelectPieces({"head"}, false);
  assert(bench->history.current().shots.size() == 1);
}

inline PiecePose Head(const Bench& bench, int frame) {
  assert(frame >= 0);
  assert(!bench.history.current().shots.empty());
  const PoseMap poses = SamplePoses(
      std::get<DollLayer>(bench.history.current().shots[0]->layers[0].content),
      Frame(frame));
  const auto found = poses.find("head");
  return found == poses.end() ? PiecePose() : found->second;
}

}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_INSPECTOR_STAGE_H_
