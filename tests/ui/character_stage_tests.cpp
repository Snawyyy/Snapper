#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QDir>
#include <QTest>

#include "bench.h"
#include "edit/character_edits.h"
#include "edit/achievements.h"
#include "ui/achievements_window.h"
#include "ui/main_window.h"
#include "ui/toast_queue.h"
#include "ui/stage_view.h"
#include "ui/theme.h"

namespace snapper {
namespace {

void Shoot(QWidget* widget, const QString& name) {
  const QString folder = qEnvironmentVariable("SNAPPER_SHOTS");
  if (folder.isEmpty()) {
    return;
  }
  QDir().mkpath(folder);
  widget->grab().save(QDir(folder).filePath(name + ".png"));
}

}  // namespace

class CharacterStageTests final : public QObject {
  Q_OBJECT

 private slots:
  void PickedCharacterShowsItsBonesAndBox();
};

void CharacterStageTests::PickedCharacterShowsItsBonesAndBox() {
  theme::Apply(qApp);
  Bench bench;
  MainWindow window(bench.All());
  window.resize(1600, 960);
  const auto shot = bench.shots.Add(0);
  QVERIFY(shot.has_value());
  QVERIFY(bench.characters.Create("Bo").has_value());
  const Character* bo = FindCharacter(bench.history.current(), "Bo");
  const PartId bones = bo->parts[0].id;
  const PartId drawing = bo->parts[1].id;
  const std::vector<QPointF> body = {{-60, -80}, {60, -80}, {60, 160}, {-60, 160}};
  const auto ids = bench.characters.AddCurve("Bo", drawing, body, true, 0.0);
  QVERIFY(ids.has_value());
  VectorShape style;
  style.fill = QColor(240, 200, 170);
  QVERIFY(bench.characters.MakeShape("Bo", drawing, *ids, style).has_value());
  const auto spine =
      bench.characters.AddBone("Bo", bones, {0, -60}, {0, 40}, BoneId());
  QVERIFY(spine.has_value());
  QVERIFY(bench.characters.AddBone("Bo", bones, {0, 40}, {0, 140}, *spine)
              .has_value());
  const auto layer = bench.stage.AddCharacter(*shot, "Bo");
  QVERIFY(layer.has_value());
  bench.selection.PickThings({Pick{*layer}}, PickMode::kReplace, *layer);
  window.SetMode(MainWindow::Mode::kPose);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QTest::qWait(50);
  Shoot(&window, "10_pose_tab");
  // L on the stage lights the table, and the View menu follows.
  auto* stage = window.findChild<StageView*>();
  QVERIFY(stage != nullptr);
  QTest::keyClick(stage, Qt::Key_L);
  QVERIFY(stage->is_light_table_on());
  QAction* light = nullptr;
  for (QAction* menu : window.menuBar()->actions()) {
    for (QAction* action : menu->menu()->actions()) {
      if (action->text() == "&Light table") {
        light = action;
      }
    }
  }
  QVERIFY(light != nullptr && light->isChecked());
  // Lighting the table earns Seeing Ghosts, shown on a gold note.
  QVERIFY(bench.achievements.IsEarned(17));
  QVERIFY(bench.toasts.queue().toasts().back().kind ==
          ToastKind::kAchievement);
  const TrackRef track{*shot, TrackKind::kLayer, *layer};
  QVERIFY(bench.pose.Move(track, Frame(0), QPointF(-160, 0)).has_value());
  QVERIFY(bench.pose.Move(track, Frame(4), QPointF(0, 0)).has_value());
  QVERIFY(bench.pose.Move(track, Frame(5), QPointF(160, 0)).has_value());
  bench.playback.Seek(Frame(4));
  QTest::qWait(50);
  Shoot(&window, "14_light_table");
  bench.toasts.Say("Undo Rotate head: Ctrl+Z");
  bench.toasts.Problem("Can't key here. Pick a layer first.");
  bench.toasts.Cheer("Saved teto.snap");
  QTest::qWait(400);
  Shoot(&window, "15_notes");
  QAction* trophies = nullptr;
  for (QAction* menu : window.menuBar()->actions()) {
    for (QAction* action : menu->menu()->actions()) {
      if (action->text() == "&Achievements...") {
        trophies = action;
      }
    }
  }
  QVERIFY(trophies != nullptr && trophies->isEnabled());
  trophies->trigger();
  auto* shelf = window.findChild<AchievementsWindow*>();
  QVERIFY(shelf != nullptr);
  QVERIFY(QTest::qWaitForWindowExposed(shelf));
  Shoot(shelf, "16_achievements");
  shelf->close();
  light->setChecked(false);
  QVERIFY(!stage->is_light_table_on());
  window.SetMode(MainWindow::Mode::kCharacter);
  QTest::qWait(50);
  Shoot(&window, "11_character_tab");
  window.SetMode(MainWindow::Mode::kVideo);
  QTest::qWait(50);
  Shoot(&window, "12_video_tab");
  window.SetMode(MainWindow::Mode::kComp);
  QTest::qWait(50);
  Shoot(&window, "13_comp_tab");
}

}  // namespace snapper

QTEST_MAIN(snapper::CharacterStageTests)
#include "character_stage_tests.moc"
