#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>

#include "edit/document_manager.h"
#include "edit/draw_manager.h"
#include "edit/history_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "edit/stage_manager.h"
#include "edit/storyboard_edits.h"
#include "edit/storyboard_manager.h"
#include "io/layer_json.h"
#include "model/storyboard.h"

namespace snapper {
namespace {

constexpr int kSide = 64;

Project OneShot() {
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

const Shot& TheShot(const HistoryManager& history) {
  assert(!history.current().shots.empty());
  assert(history.current().shots[0] != nullptr);
  return *history.current().shots[0];
}

StoryboardLayer ThreeCels() {
  StoryboardLayer board;
  board.cels = {{Frame(0), "a.png"}, {Frame(10), "b.png"},
                {Frame(20), "c.png"}};
  assert(board.cels.size() == 3);
  assert(board.cels.front().at == Frame(0));
  return board;
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
    QVERIFY(draw.Done(QString()).has_value());
  }
};

}  // namespace

class StoryboardTests final : public QObject {
  Q_OBJECT

 private slots:
  void FindsTheCelShowingAtAFrame();
  void CelsAddSwapRemoveAndMove();
  void TheActiveLayerIsPickedThenTopmost();
  void NewLayersGetFreshNames();
  void AStoryboardIsAPlainLayerInTheStack();
  void DrawingAFrameIsOneNamedStep();
  void DrawingOverSwapsThePicture();
  void WhatCantBeDrawnSaysWhy();
  void MovingAndDeletingAreOneStepEach();
  void OpacityAndExportAreSteps();
  void SavesAndLoadsWithTheProject();
};

void StoryboardTests::FindsTheCelShowingAtAFrame() {
  const StoryboardLayer board = ThreeCels();
  QCOMPARE(CelAt(board, Frame(0))->path, QString("a.png"));
  QCOMPARE(CelAt(board, Frame(9))->path, QString("a.png"));
  QCOMPARE(CelAt(board, Frame(10))->path, QString("b.png"));
  QCOMPARE(CelAt(board, Frame(500))->path, QString("c.png"));
  QCOMPARE(CelStartingAt(board, Frame(10))->path, QString("b.png"));
  QVERIFY(CelStartingAt(board, Frame(11)) == nullptr);
  QCOMPARE(CelBefore(board, Frame(10))->path, QString("a.png"));
  QVERIFY(CelBefore(board, Frame(0)) == nullptr);
  QCOMPARE(CelBefore(board, Frame(15))->path, QString("b.png"));
  QCOMPARE(CelAfter(board, Frame(10))->path, QString("c.png"));
  QVERIFY(CelAfter(board, Frame(20)) == nullptr);
  StoryboardLayer late;
  late.cels = {{Frame(5), "x.png"}};
  QVERIFY(CelAt(late, Frame(4)) == nullptr);
  QVERIFY(CelAt(StoryboardLayer(), Frame(0)) == nullptr);
}

void StoryboardTests::CelsAddSwapRemoveAndMove() {
  StoryboardLayer board = ThreeCels();
  QVERIFY(PutCel(&board, Frame(5), "n.png").has_value());
  QCOMPARE(board.cels.size(), size_t{4});
  QCOMPARE(board.cels[1].at, Frame(5));
  QVERIFY(PutCel(&board, Frame(5), "m.png").has_value());
  QCOMPARE(board.cels.size(), size_t{4});
  QCOMPARE(board.cels[1].path, QString("m.png"));
  QVERIFY(RemoveCels(&board, {Frame(5)}).has_value());
  QCOMPARE(board.cels.size(), size_t{3});
  QVERIFY(!RemoveCels(&board, {Frame(7)}).has_value());
  // A move keeps the order and refuses a clash or a spot before 0.
  QVERIFY(MoveCels(&board, {Frame(10)}, 5).has_value());
  QCOMPARE(board.cels[1].at, Frame(15));
  QVERIFY(!MoveCels(&board, {Frame(15)}, 5).has_value());
  QVERIFY(!MoveCels(&board, {Frame(0)}, -1).has_value());
  QVERIFY(MoveCels(&board, {Frame(15), Frame(20)}, -5).has_value());
  QCOMPARE(board.cels[1].at, Frame(10));
  QCOMPARE(board.cels[2].at, Frame(15));
}

void StoryboardTests::TheActiveLayerIsPickedThenTopmost() {
  Shot shot;
  shot.id = ShotId(1);
  const auto add = [&shot](int id, LayerContent content) {
    Layer layer;
    layer.id = LayerId(id);
    layer.name = QString::number(id);
    layer.content = std::move(content);
    shot.layers.push_back(layer);
  };
  QVERIFY(!ActiveStoryboard(shot, LayerId()).IsValid());
  add(1, StoryboardLayer{});
  add(2, ImageLayer{"x.png"});
  add(3, StoryboardLayer{});
  QCOMPARE(ActiveStoryboard(shot, LayerId()), LayerId(3));
  QCOMPARE(ActiveStoryboard(shot, LayerId(1)), LayerId(1));
  // A picked layer that is not a storyboard falls back to the topmost.
  QCOMPARE(ActiveStoryboard(shot, LayerId(2)), LayerId(3));
  QVERIFY(IsStagePicture(shot.layers[1]));
  QVERIFY(!IsStagePicture(shot.layers[0]));
}

void StoryboardTests::NewLayersGetFreshNames() {
  Bench bench;
  QVERIFY(bench.stage.AddStoryboard(bench.shot).has_value());
  QVERIFY(bench.stage.AddStoryboard(bench.shot).has_value());
  const auto& layers = TheShot(bench.history).layers;
  QCOMPARE(layers[0].name, QString("Storyboard"));
  QCOMPARE(layers[1].name, QString("Storyboard 2"));
  QCOMPARE(bench.history.UndoLabel(), QString("Add storyboard"));
  QVERIFY(!bench.stage.AddStoryboard(ShotId(99)).has_value());
}

void StoryboardTests::AStoryboardIsAPlainLayerInTheStack() {
  Bench bench;
  const auto first = bench.stage.AddStoryboard(bench.shot);
  const auto second = bench.stage.AddStoryboard(bench.shot);
  QVERIFY(first.has_value() && second.has_value());
  // Draw order and the eye are the layer list's.
  QVERIFY(bench.stage.Move(bench.shot, *second, 0).has_value());
  QCOMPARE(TheShot(bench.history).layers[0].id, *second);
  QVERIFY(bench.stage.SetVisible(bench.shot, *first, false).has_value());
  const Layer* hidden = FindLayer(TheShot(bench.history), *first);
  QVERIFY(!hidden->is_visible);
  QVERIFY(bench.stage.Duplicate(bench.shot, *first).has_value());
  QCOMPARE(TheShot(bench.history).layers.size(), size_t{3});
}

void StoryboardTests::DrawingAFrameIsOneNamedStep() {
  Bench bench;
  QVERIFY(bench.board.WhyNoDraw(bench.shot, Frame(5), false).isEmpty());
  QVERIFY(bench.board.DrawNew(bench.shot, Frame(5)).has_value());
  QVERIFY(bench.draw.is_drawing());
  QVERIFY(!bench.draw.is_named());
  // Nothing is in the project until Done.
  QVERIFY(TheShot(bench.history).layers.empty());
  bench.DrawDot();
  QVERIFY(!bench.draw.is_drawing());
  // With no storyboard layer, Done made one, in the same step.
  QCOMPARE(TheShot(bench.history).layers.size(), size_t{1});
  QCOMPARE(bench.Cels().cels.size(), size_t{1});
  QCOMPARE(bench.Cels().cels[0].at, Frame(5));
  const QString path = bench.Cels().cels[0].path;
  QVERIFY(QFile::exists(path));
  QVERIFY(path.startsWith(QDir(bench.dir.path()).filePath("storyboard")));
  QCOMPARE(QImage(path).pixelColor(3, 3).alpha(), 255);
  QCOMPARE(QImage(path).pixelColor(40, 40).alpha(), 0);
  QCOMPARE(bench.history.UndoLabel(), QString("Draw storyboard frame"));
  // A second frame goes to that layer, with the first one as the onion.
  QVERIFY(bench.board.DrawNew(bench.shot, Frame(12)).has_value());
  QCOMPARE(bench.draw.onions().size(), size_t{1});
  QVERIFY(bench.draw.is_onion());
  QCOMPARE(bench.draw.image().pixelColor(3, 3).alpha(), 0);
  bench.DrawDot();
  QCOMPARE(TheShot(bench.history).layers.size(), size_t{1});
  QCOMPARE(bench.Cels().cels.size(), size_t{2});
  bench.history.Undo();
  QCOMPARE(bench.Cels().cels.size(), size_t{1});
  bench.history.Undo();
  QVERIFY(TheShot(bench.history).layers.empty());
}

void StoryboardTests::DrawingOverSwapsThePicture() {
  Bench bench;
  QVERIFY(bench.board.DrawNew(bench.shot, Frame(5)).has_value());
  bench.DrawDot();
  const QString first = bench.Cels().cels[0].path;
  // Frame 9 shows the cel that starts at 5.
  QVERIFY(bench.board.DrawOver(bench.shot, Frame(9)).has_value());
  QCOMPARE(bench.draw.image().pixelColor(3, 3).alpha(), 255);
  bench.draw.Press(QPoint(20, 20), false, false);
  bench.draw.Release();
  QVERIFY(bench.draw.Done(QString()).has_value());
  QCOMPARE(bench.Cels().cels.size(), size_t{1});
  QCOMPARE(bench.Cels().cels[0].at, Frame(5));
  QVERIFY(bench.Cels().cels[0].path != first);
  QCOMPARE(bench.history.UndoLabel(), QString("Edit storyboard frame"));
  bench.history.Undo();
  QCOMPARE(bench.Cels().cels[0].path, first);
}

void StoryboardTests::WhatCantBeDrawnSaysWhy() {
  Bench bench;
  QVERIFY(bench.board.DrawNew(bench.shot, Frame(5)).has_value());
  bench.DrawDot();
  QVERIFY(!bench.board.WhyNoDraw(bench.shot, Frame(5), false).isEmpty());
  QVERIFY(!bench.board.WhyNoDraw(bench.shot, Frame(2), true).isEmpty());
  QVERIFY(!bench.board.WhyNoDraw(bench.shot, Frame(48), false).isEmpty());
  QVERIFY(!bench.board.DrawNew(bench.shot, Frame(5)).has_value());
  QVERIFY(bench.board.DrawNew(bench.shot, Frame(6)).has_value());
  // One drawing at a time.
  QVERIFY(!bench.board.WhyNoDraw(bench.shot, Frame(9), false).isEmpty());
  bench.draw.Cancel();
  QCOMPARE(bench.Cels().cels.size(), size_t{1});
}

void StoryboardTests::MovingAndDeletingAreOneStepEach() {
  Bench bench;
  QVERIFY(bench.board.DrawNew(bench.shot, Frame(5)).has_value());
  bench.DrawDot();
  QVERIFY(bench.board.DrawNew(bench.shot, Frame(8)).has_value());
  bench.DrawDot();
  const LayerId layer = bench.Sheet().id;
  QVERIFY(!bench.board.MoveCels(bench.shot, layer, {}, 1).has_value());
  QVERIFY(!bench.board.WhyNoDelete(bench.shot).isEmpty());
  bench.board.Pick(bench.shot, layer, {Frame(5)}, PickMode::kReplace);
  QVERIFY(bench.board.WhyNoDelete(bench.shot).isEmpty());
  QVERIFY(bench.board.MoveCels(bench.shot, layer, {Frame(5)}, 1)
              .has_value());
  QCOMPARE(bench.Cels().cels[0].at, Frame(6));
  QCOMPARE(bench.history.UndoLabel(), QString("Move storyboard frame"));
  // The pick follows the moved frame.
  QVERIFY(bench.board.Picked(bench.shot, layer).contains(Frame(6)));
  // Landing on a frame that stays is refused and changes nothing.
  QVERIFY(!bench.board.MoveCels(bench.shot, layer, {Frame(6)}, 2)
               .has_value());
  QCOMPARE(bench.Cels().cels[0].at, Frame(6));
  QVERIFY(bench.board.DeleteCels(bench.shot, layer, {Frame(6), Frame(8)})
              .has_value());
  QVERIFY(bench.Cels().cels.empty());
  QCOMPARE(bench.history.UndoLabel(), QString("Delete 2 storyboard frames"));
  QVERIFY(bench.board.Picked(bench.shot, layer).empty());
  bench.history.Undo();
  QCOMPARE(bench.Cels().cels.size(), size_t{2});
}

void StoryboardTests::OpacityAndExportAreSteps() {
  Bench bench;
  const auto id = bench.stage.AddStoryboard(bench.shot);
  QVERIFY(id.has_value());
  QCOMPARE(bench.Cels().opacity, kDefaultStoryboardOpacity);
  QVERIFY(!bench.Cels().is_exported);
  QVERIFY(bench.board.SetOpacity(bench.shot, *id, 0.25).has_value());
  QCOMPARE(bench.Cels().opacity, 0.25);
  QVERIFY(bench.board.SetOpacity(bench.shot, *id, 7.0).has_value());
  QCOMPARE(bench.Cels().opacity, 1.0);
  QVERIFY(bench.board.SetExported(bench.shot, *id, true).has_value());
  QVERIFY(bench.Cels().is_exported);
  QCOMPARE(bench.history.UndoLabel(), QString("Export storyboard"));
  bench.history.Undo();
  QVERIFY(!bench.Cels().is_exported);
  QVERIFY(!bench.board.SetOpacity(bench.shot, LayerId(99), 0.5).has_value());
}

void StoryboardTests::SavesAndLoadsWithTheProject() {
  Shot shot;
  shot.id = ShotId(1);
  Layer layer;
  layer.id = LayerId(4);
  layer.name = "Storyboard";
  StoryboardLayer board = ThreeCels();
  board.opacity = 0.35;
  board.is_exported = true;
  layer.content = board;
  layer.is_visible = false;
  shot.layers.push_back(layer);
  JsonIssues issues;
  const Shot loaded = ShotFromJson(ShotToJson(shot), &issues);
  QVERIFY(!issues.HasIssue());
  QCOMPARE(loaded.layers.size(), size_t{1});
  QVERIFY(loaded.layers[0] == layer);
  // An old project has no storyboard keys in its layers: nothing breaks,
  // and a layer saved without the fields reads with the defaults.
  QJsonObject bare = ShotToJson(shot);
  QJsonArray layers = bare.value("layers").toArray();
  QJsonObject only = layers[0].toObject();
  only.remove("opacity");
  only.remove("exported");
  only.remove("cels");
  layers[0] = only;
  bare.insert("layers", layers);
  const Shot defaults = ShotFromJson(bare, &issues);
  const auto* read = std::get_if<StoryboardLayer>(&defaults.layers[0].content);
  QVERIFY(read != nullptr);
  QVERIFY(read->cels.empty());
  QCOMPARE(read->opacity, kDefaultStoryboardOpacity);
  QVERIFY(!read->is_exported);
  Shot empty;
  empty.id = ShotId(2);
  QVERIFY(ShotFromJson(ShotToJson(empty), &issues).layers.empty());
}

}  // namespace snapper

QTEST_MAIN(snapper::StoryboardTests)
#include "storyboard_tests.moc"
