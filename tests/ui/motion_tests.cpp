#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "ui/main_window.h"
#include "ui/motion.h"
#include "ui/motion_menu.h"
#include "ui/motion_table.h"

namespace snapper {
namespace {

constexpr MotionKind kKinds[] = {MotionKind::kPress, MotionKind::kHover,
                                 MotionKind::kPop, MotionKind::kMove,
                                 MotionKind::kCap};
constexpr MotionMode kModes[] = {MotionMode::kFull, MotionMode::kCalm,
                                 MotionMode::kOff};

// The checked item of the View > Motion menu, or empty.
QString CheckedMotion(QMainWindow* window) {
  assert(window != nullptr);
  assert(window->menuBar() != nullptr);
  for (QAction* top : window->menuBar()->actions()) {
    const bool is_view = top->text() == QStringLiteral("&View");
    if (is_view) {
      for (QAction* item : top->menu()->actions().first()->menu()->actions()) {
        const bool is_checked = item->isChecked();
        if (is_checked) {
          return item->text();
        }
      }
    }
  }
  return QString();
}

}  // namespace

class MotionTests final : public QObject {
  Q_OBJECT

 private slots:
  void FullTableMatchesTheDesign();
  void CalmIsShorterAndOffIsZero();
  void PopsOvershootOnlyInFull();
  void NothingIsLinear();
  void ModeIsSavedAndReloaded();
  void UnknownSavedModeFallsBackToFull();
  void AnimateEndsOnTheTarget();
  void MenuShowsAndChangesTheMode();
};

void MotionTests::FullTableMatchesTheDesign() {
  QCOMPARE(MotionMs(MotionMode::kFull, MotionKind::kPress), 80);
  QCOMPARE(MotionMs(MotionMode::kFull, MotionKind::kHover), 100);
  QCOMPARE(MotionMs(MotionMode::kFull, MotionKind::kPop), 150);
  QCOMPARE(MotionMs(MotionMode::kFull, MotionKind::kMove), 200);
  QCOMPARE(MotionMs(MotionMode::kFull, MotionKind::kCap), 300);
}

void MotionTests::CalmIsShorterAndOffIsZero() {
  QCOMPARE(MotionMs(MotionMode::kCalm, MotionKind::kPress), 60);
  QCOMPARE(MotionMs(MotionMode::kCalm, MotionKind::kHover), 80);
  QCOMPARE(MotionMs(MotionMode::kCalm, MotionKind::kPop), 100);
  QCOMPARE(MotionMs(MotionMode::kCalm, MotionKind::kMove), 120);
  for (const MotionKind kind : kKinds) {
    QVERIFY(MotionMs(MotionMode::kCalm, kind) <
            MotionMs(MotionMode::kFull, kind));
    QCOMPARE(MotionMs(MotionMode::kOff, kind), 0);
  }
}

void MotionTests::PopsOvershootOnlyInFull() {
  const QEasingCurve full = MotionEasing(MotionMode::kFull, MotionKind::kPop);
  const QEasingCurve calm = MotionEasing(MotionMode::kCalm, MotionKind::kPop);
  qreal full_peak = 0.0;
  qreal calm_peak = 0.0;
  for (int step = 0; step <= 100; ++step) {
    const qreal t = step / 100.0;
    full_peak = qMax(full_peak, full.valueForProgress(t));
    calm_peak = qMax(calm_peak, calm.valueForProgress(t));
  }
  QVERIFY(full_peak > 1.05 && full_peak < 1.15);
  QVERIFY(calm_peak <= 1.0001);
  QCOMPARE(full.valueForProgress(1.0), 1.0);
  QCOMPARE(full.valueForProgress(0.0), 0.0);
}

void MotionTests::NothingIsLinear() {
  for (const MotionMode mode : kModes) {
    for (const MotionKind kind : kKinds) {
      const QEasingCurve curve = MotionEasing(mode, kind);
      QVERIFY(curve.valueForProgress(0.25) > 0.3);
      QVERIFY(curve.valueForProgress(0.5) > 0.6);
    }
  }
}

void MotionTests::ModeIsSavedAndReloaded() {
  QTemporaryDir dir;
  const QString path = dir.filePath("settings.ini");
  MotionManager first(path);
  QCOMPARE(first.mode(), MotionMode::kFull);
  QSignalSpy spy(&first, &MotionManager::ModeChanged);
  first.SetMode(MotionMode::kCalm);
  first.SetMode(MotionMode::kCalm);
  QCOMPARE(spy.count(), 1);
  QCOMPARE(first.duration(MotionKind::kMove), 120);
  const MotionManager second(path);
  QCOMPARE(second.mode(), MotionMode::kCalm);
}

void MotionTests::UnknownSavedModeFallsBackToFull() {
  QCOMPARE(MotionModeFromKey("wobbly", MotionMode::kFull), MotionMode::kFull);
  for (const MotionMode mode : kModes) {
    QCOMPARE(MotionModeFromKey(MotionModeKey(mode), MotionMode::kFull), mode);
  }
}

void MotionTests::AnimateEndsOnTheTarget() {
  QTemporaryDir dir;
  MotionManager motion(dir.filePath("settings.ini"));
  QObject owner;
  motion.SetMode(MotionMode::kOff);
  double last = -1.0;
  motion.Animate(&owner, MotionKind::kMove, 0.0, 10.0,
                 [&last](const QVariant& value) { last = value.toDouble(); });
  QCOMPARE(last, 10.0);
  motion.SetMode(MotionMode::kFull);
  double full_last = -1.0;
  motion.Animate(&owner, MotionKind::kHover, 0.0, 5.0,
                 [&full_last](const QVariant& value) {
                   full_last = value.toDouble();
                 });
  QTRY_COMPARE(full_last, 5.0);
}

void MotionTests::MenuShowsAndChangesTheMode() {
  Bench rig;
  MainWindow window(rig.All());
  QCOMPARE(CheckedMotion(&window), QString("&Full"));
  rig.motion.SetMode(MotionMode::kOff);
  QCOMPARE(CheckedMotion(&window), QString("&Off"));
  for (QAction* top : window.menuBar()->actions()) {
    const bool is_view = top->text() == QStringLiteral("&View");
    if (is_view) {
      QAction* calm = top->menu()->actions().first()->menu()->actions()[1];
      calm->trigger();
    }
  }
  QCOMPARE(rig.motion.mode(), MotionMode::kCalm);
  QCOMPARE(CheckedMotion(&window), QString("&Calm"));
}

}  // namespace snapper

QTEST_MAIN(snapper::MotionTests)
#include "motion_tests.moc"
