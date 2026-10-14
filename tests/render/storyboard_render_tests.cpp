#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>

#include "model/project.h"
#include "model/storyboard.h"
#include "render/frame_renderer.h"

namespace snapper {
namespace {

constexpr int kSide = 64;

Project OneShot() {
  Project project;
  project.canvas.width = kSide;
  project.canvas.height = kSide;
  Shot shot;
  shot.id = ShotId(1);
  project.shots = {std::make_shared<const Shot>(shot)};
  project.next_shot_id = 2;
  assert(project.shots.size() == 1);
  assert(IsValidCanvas(project.canvas));
  return project;
}

// A solid-red picture the size of the canvas, saved at path.
void SaveRed(const QString& path) {
  QImage image(kSide, kSide, QImage::Format_ARGB32);
  image.fill(Qt::red);
  const bool is_saved = image.save(path);
  assert(is_saved);
  assert(QFile::exists(path));
}

}  // namespace

class StoryboardRenderTests final : public QObject {
  Q_OBJECT

 private slots:
  void ExportLeavesItOutUnlessTicked();
};

void StoryboardRenderTests::ExportLeavesItOutUnlessTicked() {
  QTemporaryDir dir;
  const QString red = QDir(dir.path()).filePath("red.png");
  SaveRed(red);
  Project project = OneShot();
  Shot shot = *project.shots[0];
  shot.background = Qt::white;
  Layer layer;
  layer.id = LayerId(1);
  layer.name = "Storyboard";
  StoryboardLayer board;
  board.cels = {{Frame(4), red}};
  board.opacity = 1.0;
  layer.content = board;
  shot.layers.push_back(layer);
  const auto at = [&](const Shot& drawn, Frame frame, bool is_shown) {
    FrameRenderer renderer;
    renderer.SetStoryboardShown(is_shown);
    return renderer.RenderShot(project, drawn, frame, 1.0)
        .pixelColor(10, 10);
  };
  // The stage and the preview show it from its first frame; export not.
  QCOMPARE(at(shot, Frame(5), true), QColor(Qt::red));
  QCOMPARE(at(shot, Frame(3), true), QColor(Qt::white));
  QCOMPARE(at(shot, Frame(5), false), QColor(Qt::white));
  Shot ticked = shot;
  std::get<StoryboardLayer>(ticked.layers[0].content).is_exported = true;
  QCOMPARE(at(ticked, Frame(5), false), QColor(Qt::red));
  // The layer's eye hides it everywhere.
  ticked.layers[0].is_visible = false;
  QCOMPARE(at(ticked, Frame(5), true), QColor(Qt::white));
  QCOMPARE(at(ticked, Frame(5), false), QColor(Qt::white));
  // It lies in the stack: a layer above covers it.
  Shot covered = shot;
  Layer cover;
  cover.id = LayerId(2);
  cover.name = "Fill";
  EffectLayer fill;
  fill.kind = EffectKind::kFill;
  fill.color = Qt::blue;
  cover.content = fill;
  covered.layers.push_back(cover);
  QCOMPARE(at(covered, Frame(5), true), QColor(Qt::blue));
}

}  // namespace snapper

QTEST_MAIN(snapper::StoryboardRenderTests)
#include "storyboard_render_tests.moc"
