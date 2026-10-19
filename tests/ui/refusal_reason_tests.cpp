#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTabBar>
#include <QTest>

#include <cassert>
#include <memory>

#include "bench.h"
#include "ui/main_window.h"
#include "ui/refusal_shaker.h"

namespace snapper {
namespace {

// A project with one character layer and nothing picked.
Project OneLayerProject() {
  constexpr int kLayerId = 1;
  assert(kLayerId > 0);
  Layer layer;
  layer.id = LayerId(kLayerId);
  layer.content = CharacterLayer{"Dot", {}, false};
  Shot shot;
  shot.id = ShotId(1);
  shot.layers = {layer};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  assert(project.shots.size() == 1);
  return project;
}

// Every menu under the menu bar, submenus included.
QList<QMenu*> AllMenus(QMainWindow* window) {
  assert(window != nullptr);
  assert(window->menuBar() != nullptr);
  constexpr int kMaxMenus = 64;
  QList<QMenu*> found;
  for (QAction* top : window->menuBar()->actions()) {
    found << top->menu();
  }
  for (int i = 0; i < found.size() && i < kMaxMenus; ++i) {
    for (QAction* item : found[i]->actions()) {
      const bool is_submenu = item->menu() != nullptr;
      if (is_submenu) {
        found << item->menu();
      }
    }
  }
  return found;
}

// Names of the controls and menu items that are greyed with no reason.
QStringList SilentlyDisabled(QMainWindow* window) {
  assert(window != nullptr);
  assert(window->menuBar() != nullptr);
  QStringList silent;
  const auto widgets = window->findChildren<QWidget*>();
  for (const QWidget* widget : widgets) {
    // A tab bar's own scroll arrows grey at either end; Qt's, not ours.
    const bool is_tab_arrow =
        qobject_cast<const QTabBar*>(widget->parentWidget()) != nullptr;
    const bool is_forced = widget->testAttribute(Qt::WA_ForceDisabled);
    const bool is_silent =
        is_forced && !is_tab_arrow && widget->toolTip().isEmpty();
    if (is_silent) {
      const QWidget* parent = widget->parentWidget();
      silent << QStringLiteral("widget %1 %2 in %3 %4")
                    .arg(widget->metaObject()->className(),
                         widget->objectName(),
                         parent != nullptr ? parent->metaObject()->className()
                                           : "nothing",
                         parent != nullptr ? parent->objectName() : QString());
    }
  }
  QList<QAction*> actions = window->findChildren<QAction*>();
  for (const QMenu* menu : AllMenus(window)) {
    actions << menu->actions();
  }
  for (const QAction* action : actions) {
    const bool is_silent = !action->isEnabled() && !action->isSeparator() &&
                           action->toolTip().isEmpty();
    if (is_silent) {
      silent << QStringLiteral("action %1").arg(action->text());
    }
  }
  return silent;
}

}  // namespace

class RefusalReasonTests final : public QObject {
  Q_OBJECT

 private slots:
  void EmptyProjectHasNoSilentDisabledControl();
  void NothingPickedHasNoSilentDisabledControl();
  void MenusShowTheirTooltips();
  void ReasonFallsBackToAGenericOne();
  void ReasonComesFromTheNearestParent();
};

void RefusalReasonTests::EmptyProjectHasNoSilentDisabledControl() {
  Bench rig;
  assert(rig.history.current().shots.empty());
  MainWindow window(rig.All());
  const QStringList silent = SilentlyDisabled(&window);
  QVERIFY2(silent.isEmpty(), qPrintable(silent.join("; ")));
}

void RefusalReasonTests::NothingPickedHasNoSilentDisabledControl() {
  Bench rig;
  rig.history.Reset(OneLayerProject());
  MainWindow window(rig.All());
  const QStringList silent = SilentlyDisabled(&window);
  QVERIFY2(silent.isEmpty(), qPrintable(silent.join("; ")));
  window.SetMode(MainWindow::Mode::kVideo);
  const QStringList video_silent = SilentlyDisabled(&window);
  QVERIFY2(video_silent.isEmpty(), qPrintable(video_silent.join("; ")));
}

void RefusalReasonTests::MenusShowTheirTooltips() {
  Bench rig;
  MainWindow window(rig.All());
  const QList<QMenu*> menus = AllMenus(&window);
  QVERIFY(!menus.isEmpty());
  for (const QMenu* menu : menus) {
    QVERIFY2(menu->toolTipsVisible(), qPrintable(menu->title()));
  }
}

void RefusalReasonTests::ReasonFallsBackToAGenericOne() {
  QPushButton plain("Plain");
  QVERIFY(!RefusalReason(&plain).isEmpty());
  plain.setToolTip("Because.");
  QCOMPARE(RefusalReason(&plain), QString("Because."));
}

void RefusalReasonTests::ReasonComesFromTheNearestParent() {
  assert(qApp != nullptr);
  QWidget parent;
  parent.setToolTip("Parent says no.");
  QLabel child("x", &parent);
  QCOMPARE(RefusalReason(&child), QString("Parent says no."));
}

}  // namespace snapper

QTEST_MAIN(snapper::RefusalReasonTests)
#include "refusal_reason_tests.moc"
