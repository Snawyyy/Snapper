#include <QDir>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QLabel>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>

#include "bench.h"
#include "ui/bump_label.h"
#include "ui/main_window.h"
#include "ui/mode_tabs.h"
#include "ui/new_project_dialog.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The menu bar action whose text starts with text, one menu deep.
QAction* FindAction(QMainWindow* window, const QString& text) {
  for (QAction* top : window->menuBar()->actions()) {
    const QList<QAction*> items =
        top->menu() != nullptr ? top->menu()->actions() : QList<QAction*>();
    for (QAction* action : items) {
      const bool is_match = action->text().startsWith(text);
      if (is_match) {
        return action;
      }
    }
  }
  return nullptr;
}

}  // namespace

class WindowTests final : public QObject {
  Q_OBJECT

 private slots:
  void TitleShowsNameDirtAndPlace();
  void UndoSaysWhatAndWhyNot();
  void NewProjectDialogChecksItsFields();
  void DrawsInTheTheme();
  void VideoTabRunsTheReel();
  void TabsStayPutInEveryMode();
  void TopBarUndoNamesTheStep();
  void TopBarTransportDrivesPlayback();
  void RedBarSlidesToTheTab();
  void TabsWearIcons();
  void CounterHopsOnAStepNotInOff();
};

void WindowTests::TitleShowsNameDirtAndPlace() {
  Bench rig;
  MainWindow window(rig.All());
  QVERIFY(window.windowTitle().contains("Untitled"));
  QVERIFY(window.windowTitle().contains("not saved yet"));
  QVERIFY(!window.isWindowModified());
  QVERIFY(rig.document.Rename("Teto").has_value());
  QVERIFY(window.windowTitle().startsWith("Teto"));
  QVERIFY(window.isWindowModified());
  QVERIFY(rig.document.SaveAs(QDir(rig.dir.path()).filePath("t"))
              .has_value());
  QVERIFY(window.windowTitle().contains("t.snapper"));
  QVERIFY(!window.isWindowModified());
  QCOMPARE(window.mode(), MainWindow::Mode::kPose);
}

void WindowTests::UndoSaysWhatAndWhyNot() {
  Bench rig;
  MainWindow window(rig.All());
  QAction* undo = FindAction(&window, "&Undo");
  QVERIFY(undo != nullptr);
  QVERIFY(!undo->isEnabled());
  QCOMPARE(undo->toolTip(), QString("Nothing to undo"));
  QVERIFY(rig.shots.Add(-1).has_value());
  QVERIFY(undo->isEnabled());
  QCOMPARE(undo->text(), QString("&Undo Add shot"));
  QAction* remove_song = FindAction(&window, "Remove song");
  QVERIFY(remove_song != nullptr && !remove_song->isEnabled());
}

void WindowTests::NewProjectDialogChecksItsFields() {
  NewProjectDialog dialog(nullptr);
  QVERIFY(dialog.WhyNotReady().isEmpty());
  QCOMPARE(dialog.canvas(), (CanvasSize{1920, 1080}));
  dialog.SetCanvas({1081, 1080});
  QVERIFY(dialog.WhyNotReady().contains("even"));
  auto* edit = dialog.findChild<QLineEdit*>();
  QVERIFY(edit != nullptr);
  edit->clear();
  QVERIFY(dialog.WhyNotReady().contains("name"));
  const auto buttons = dialog.findChildren<QPushButton*>();
  const bool has_disabled_ok = std::any_of(
      buttons.begin(), buttons.end(),
      [](QPushButton* button) { return !button->isEnabled(); });
  QVERIFY(has_disabled_ok);
}

void WindowTests::DrawsInTheTheme() {
  theme::Apply(qApp);
  Bench rig;
  QImage red(200, 300, QImage::Format_ARGB32);
  red.fill(QColor(220, 60, 90));
  QVERIFY(red.save(QDir(rig.dir.path()).filePath("body.png")));
  Doll doll;
  doll.folder = rig.dir.path();
  doll.art.pieces = {{"body", {"body.png"}, 0, {-100, -150}, {200, 300}}};
  doll.rig.pieces = {{"body", "", {100, 20}, 0, -1, {}}};
  Layer layer;
  layer.id = LayerId(1);
  layer.content = DollLayer{"Dot", {}, false};
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.dolls["Dot"] = std::make_shared<const Doll>(doll);
  project.shots = {std::make_shared<const Shot>(shot)};
  rig.history.Reset(project);
  rig.selection.SelectLayer(LayerId(1));
  rig.selection.SelectPieces({"body"}, false);
  MainWindow window(rig.All());
  window.resize(900, 600);
  const QImage picture = window.grab().toImage();
  // Kept beside the test binary so the look can be checked by eye.
  picture.save(QDir(QT_TESTCASE_BUILDDIR).filePath("window.png"));
  const QColor face = picture.pixelColor(picture.width() - 3, 3);
  QVERIFY(face.lightness() < 90);
}

void WindowTests::VideoTabRunsTheReel() {
  Bench rig;
  MainWindow window(rig.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* cast = window.findChild<QDockWidget*>("cast");
  QVERIFY(cast != nullptr && cast->isVisible());
  window.SetMode(MainWindow::Mode::kVideo);
  QCOMPARE(rig.playback.timeline(), Timeline::kReel);
  QVERIFY(!cast->isVisible());
  window.SetMode(MainWindow::Mode::kPose);
  QCOMPARE(rig.playback.timeline(), Timeline::kShots);
  QVERIFY(cast->isVisible());
}

void WindowTests::TabsStayPutInEveryMode() {
  Bench rig;
  MainWindow window(rig.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* tabs = window.findChild<QTabBar*>("modes");
  QVERIFY(tabs != nullptr);
  const QPoint pose_at = tabs->mapTo(&window, QPoint(0, 0));
  constexpr MainWindow::Mode kModes[] = {
      MainWindow::Mode::kRig, MainWindow::Mode::kVideo,
      MainWindow::Mode::kPose};
  for (const MainWindow::Mode mode : kModes) {
    window.SetMode(mode);
    // Docks hide and show through the layout, so let it settle.
    QCoreApplication::processEvents();
    QCOMPARE(window.mode(), mode);
    QCOMPARE(tabs->mapTo(&window, QPoint(0, 0)), pose_at);
  }
}

void WindowTests::TopBarUndoNamesTheStep() {
  Bench rig;
  MainWindow window(rig.All());
  auto* undo = window.findChild<QPushButton*>("undo");
  auto* redo = window.findChild<QPushButton*>("redo");
  QVERIFY(undo != nullptr && redo != nullptr);
  QVERIFY(!undo->isEnabled());
  QCOMPARE(undo->toolTip(), QString("Nothing to undo"));
  QCOMPARE(redo->toolTip(), QString("Nothing to redo"));
  QVERIFY(rig.shots.Add(-1).has_value());
  QVERIFY(undo->isEnabled());
  QCOMPARE(undo->text(), QString("Undo Add shot"));
  undo->click();
  QVERIFY(!undo->isEnabled());
  QVERIFY(redo->isEnabled());
  QCOMPARE(redo->text(), QString("Redo Add shot"));
  redo->click();
  QCOMPARE(undo->text(), QString("Undo Add shot"));
}

void WindowTests::TopBarTransportDrivesPlayback() {
  Bench rig;
  MainWindow window(rig.All());
  QVERIFY(rig.shots.Add(-1).has_value());
  auto* next = window.findChild<QPushButton*>("next");
  auto* last = window.findChild<QPushButton*>("last");
  auto* first = window.findChild<QPushButton*>("first");
  auto* play = window.findChild<QPushButton*>("play");
  auto* counter = window.findChild<QLabel*>("counter");
  QVERIFY(next && last && first && play && counter);
  QCOMPARE(counter->text(), QString("F 000"));
  next->click();
  QCOMPARE(rig.playback.frame().index(), 1);
  QCOMPARE(counter->text(), QString("F 001"));
  last->click();
  QVERIFY(rig.playback.frame().index() > 1);
  first->click();
  QCOMPARE(rig.playback.frame().index(), 0);
  QCOMPARE(play->text(), QString("Play"));
  play->click();
  QVERIFY(rig.playback.IsPlaying());
  QCOMPARE(play->text(), QString("Pause"));
  QVERIFY(play->isChecked());
  play->click();
  QVERIFY(!rig.playback.IsPlaying());
  QCOMPARE(play->text(), QString("Play"));
}

void WindowTests::RedBarSlidesToTheTab() {
  Bench rig;
  rig.motion.SetMode(MotionMode::kOff);
  MainWindow window(rig.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  auto* tabs = window.findChild<ModeTabs*>("modes");
  QVERIFY(tabs != nullptr);
  window.SetMode(MainWindow::Mode::kVideo);
  const QRectF bar = tabs->bar();
  QVERIFY(bar.left() >= tabs->tabRect(2).left());
  QVERIFY(bar.right() <= tabs->tabRect(2).right());
  window.SetMode(MainWindow::Mode::kPose);
  QVERIFY(tabs->bar().left() < bar.left());
}

void WindowTests::CounterHopsOnAStepNotInOff() {
  Bench rig;
  MainWindow window(rig.All());
  QVERIFY(rig.shots.Add(-1).has_value());
  auto* next = window.findChild<QPushButton*>("next");
  auto* counter = window.findChild<BumpLabel*>("counter");
  QVERIFY(next != nullptr && counter != nullptr);
  QTRY_COMPARE(counter->lift(), 0);
  next->click();
  QVERIFY(counter->lift() > 0);
  QTRY_COMPARE(counter->lift(), 0);
  rig.motion.SetMode(MotionMode::kOff);
  next->click();
  QCOMPARE(counter->text(), QString("F 002"));
  QCOMPARE(counter->lift(), 0);
}

void WindowTests::TabsWearIcons() {
  Bench rig;
  MainWindow window(rig.All());
  auto* tabs = window.findChild<ModeTabs*>("modes");
  QVERIFY(tabs != nullptr);
  for (int i = 0; i < tabs->count(); ++i) {
    QVERIFY(!tabs->tabIcon(i).isNull());
  }
  const QImage rest = tabs->tabIcon(1).pixmap(16, 16).toImage();
  window.SetMode(MainWindow::Mode::kRig);
  const QImage picked = tabs->tabIcon(1).pixmap(16, 16).toImage();
  QVERIFY(rest != picked);
}

}  // namespace snapper

QTEST_MAIN(snapper::WindowTests)
#include "window_tests.moc"
