#ifndef SNAPPER_TESTS_UI_INSPECTOR_STAGE_H_
#define SNAPPER_TESTS_UI_INSPECTOR_STAGE_H_

#include <cassert>
#include <memory>
#include <variant>

#include "anim/sampler.h"
#include "bench.h"
#include "edit/character_edits.h"

namespace snapper {

// One shot with Bob (a character, picked) and a text layer.
inline const ShotId kShot(1);

inline void Stage(Bench* bench) {
  assert(bench != nullptr);
  Layer bob;
  bob.id = LayerId(1);
  bob.name = "Bob";
  bob.content = CharacterLayer{"Bob", {}, false};
  Layer words;
  words.id = LayerId(2);
  words.name = "Words";
  words.content = TextLayer{};
  std::get<TextLayer>(words.content).text = "Hi";
  Shot shot;
  shot.id = kShot;
  shot.layers = {bob, words};
  Project project;
  project.characters["Bob"] =
      std::make_shared<const Character>(NewCharacter("Bob"));
  project.next_layer_id = 3;
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  bench->selection.SelectLayer(LayerId(1));
  assert(bench->history.current().shots.size() == 1);
}

// Bob's layer pose at frame.
inline LayerPose Head(const Bench& bench, int frame) {
  assert(frame >= 0);
  assert(!bench.history.current().shots.empty());
  return Sample(bench.history.current().shots[0]->layers[0].transform,
                Frame(frame), LayerPose());
}

}  // namespace snapper

#endif  // SNAPPER_TESTS_UI_INSPECTOR_STAGE_H_
