#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTest>
#include <QTimer>

#include <functional>

#include "bench.h"
#include "ui/stage_view.h"

namespace snapper {
namespace {

// A 100x100 shot: a red doll Dot in the middle and a blue box picture at
// the top left, its drawings in the bench's folder.
void Stage(Bench* bench) {
  QImage red(20, 20, QImage::Format_ARGB32);
  red.fill(Qt::red);
  QVERIFY(red.save(QDir(bench->dir.path()).filePath("body.png")));
  QImage blue(20, 20, QImage::Format_ARGB32);
  blue.fill(Qt::blue);
  const QString box_path = QDir(bench->dir.path()).filePath("box.png");
  QVERIFY(blue.save(box_path));
  Doll doll;
  doll.folder = bench->dir.path();
  doll.art.pieces = {{"body", {"body.png"}, 0, {-10, -10}, {20, 20}}};
  doll.rig.pieces = {{"body", "", {10, 10}, 0, -1, {}}};
  Layer dot;
  dot.id = LayerId(1);
  dot.name = "Dot";
  dot.content = DollLayer{"Dot", {}, false};
  Layer box;
  box.id = LayerId(2);
  box.name = "Box";
  box.content = ImageLayer{box_path};
  PiecePose corner;
  corner.offset = QPointF(-35, -35);
  SetKey(&box.transform, {Frame(0), corner, Ease::kStep});
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {dot, box};
  Project project;
  project.canvas = {100, 100};
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  bench->selection.SelectShot(ShotId(1));
}

// Right-clicks the stage at the canvas point at, then hands the menu
// that opens to use and closes it.
void RightClick(StageView* stage, QPointF at,
                const std::function<void(QMenu*)>& use) {
  const auto frame = stage->CurrentFrame();
  QVERIFY(frame.has_value());
  const QPoint where = (frame->corner + at * frame->scale).toPoint();
  bool was_seen = false;
  QTimer::singleShot(0, stage, [&use, &was_seen] {
    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
    was_seen = menu != nullptr;
    if (menu != nullptr) {
      use(menu);
      menu->close();
    }
  });
  QContextMenuEvent event(QContextMenuEvent::Mouse, where,
                          stage->mapToGlobal(where));
  QApplication::sendEvent(stage, &event);
  QVERIFY(was_seen);
}

QAction* Find(QMenu* menu, const QString& text) {
  for (QAction* action : menu->actions()) {
    const bool is_match = action->text() == text;
    if (is_match) {
      return action;
    }
  }
  return nullptr;
}

}  // namespace

class LinkUiTests final : public QObject {
  Q_OBJECT

 private slots:
  void RightClickLinksThePick();
  void LinkingToItselfIsGreyedOut();
};

void LinkUiTests::RightClickLinksThePick() {
  Bench bench;
  Stage(&bench);
  StageView stage(bench.All());
  stage.resize(400, 400);
  stage.show();
  QVERIFY(QTest::qWaitForWindowExposed(&stage));
  bench.selection.PickThings({Pick{LayerId(1), "body"}}, PickMode::kReplace,
                             LayerId(1));
  RightClick(&stage, QPointF(15, 15), [](QMenu* menu) {
    QAction* link = Find(menu, "Link movement to Box");
    QVERIFY(link != nullptr);
    QVERIFY(link->isEnabled());
    link->trigger();
  });
  const Link* made = FindLink(*bench.history.current().shots[0],
                              LinkEnd{LayerId(1), "body"});
  QVERIFY(made != nullptr);
  QCOMPARE(made->leader, (LinkEnd{LayerId(2), {}}));
  RightClick(&stage, QPointF(15, 15), [](QMenu* menu) {
    QAction* unlink = Find(menu, "Unlink movement");
    QVERIFY(unlink != nullptr);
    unlink->trigger();
  });
  QVERIFY(bench.history.current().shots[0]->links.empty());
}

void LinkUiTests::LinkingToItselfIsGreyedOut() {
  Bench bench;
  Stage(&bench);
  StageView stage(bench.All());
  stage.resize(400, 400);
  stage.show();
  QVERIFY(QTest::qWaitForWindowExposed(&stage));
  bench.selection.PickThings({Pick{LayerId(1), {}}}, PickMode::kReplace,
                             LayerId(1));
  RightClick(&stage, QPointF(50, 50), [](QMenu* menu) {
    QAction* whole = Find(menu, "Link movement to Dot");
    QAction* piece = Find(menu, "Link movement to Dot's body");
    QVERIFY(whole != nullptr && piece != nullptr);
    QVERIFY(!whole->isEnabled());
    QVERIFY(!whole->toolTip().isEmpty());
    QVERIFY(Find(menu, "Unlink movement") == nullptr);
  });
}

}  // namespace snapper

QTEST_MAIN(snapper::LinkUiTests)
#include "link_ui_tests.moc"
