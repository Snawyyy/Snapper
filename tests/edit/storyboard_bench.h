#ifndef SNAPPER_TESTS_EDIT_STORYBOARD_BENCH_H_
#define SNAPPER_TESTS_EDIT_STORYBOARD_BENCH_H_

#include <QPoint>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>
#include <variant>

#include "edit/document_manager.h"
#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "edit/storyboard_manager.h"

namespace snapper {

constexpr int kSide = 64;

inline Project OneShot() {
  Project project;
  project.canvas.width = kSide;
  project.canvas.height = kSide;
  Shot shot;
  shot.id = ShotId(1);
  shot.name = "Opening";
  project.shots = {std::make_shared<const Shot>(shot)};
  project.next_shot_id = 2;
  assert(project.shots.size() == 1);
  assert(IsValidCanvas(project.canvas));
  return project;
}

inline const Shot& TheShot(const HistoryManager& history) {
  assert(!history.current().shots.empty());
  assert(history.current().shots[0] != nullptr);
  return *history.current().shots[0];
}

// Every manager the drawing path needs, over a scratch folder.
struct Bench final {
  QTemporaryDir dir;
  HistoryManager history{OneShot()};
  DocumentManager document{&history, dir.path()};
  PlaybackManager playback{&history};
  DrawManager draw{&history, &playback, nullptr};
  SelectionManager selection{&history};
  StageManager stage{&history};
  StoryboardManager board{&history, &document, &draw, &selection,
                          dir.path()};
  ShotId shot{1};

  // The first storyboard layer of the shot.
  const Layer& Sheet() const { return TheShot(history).layers.front(); }
  const StoryboardLayer& Cels() const {
    const auto* found = std::get_if<StoryboardLayer>(&Sheet().content);
    assert(found != nullptr);
    return *found;
  }
  // Draws one dot and presses Done.
  void DrawDot() {
    draw.Press(QPoint(3, 3), false, false);
    draw.Release();
    QVERIFY(draw.Done().has_value());
  }
};

}  // namespace snapper

#endif  // SNAPPER_TESTS_EDIT_STORYBOARD_BENCH_H_
