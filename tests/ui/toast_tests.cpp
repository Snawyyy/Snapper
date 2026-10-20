#include <QKeyEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "ui/main_window.h"
#include "ui/shot_strip.h"
#include "ui/toast_manager.h"
#include "ui/toast_queue.h"
#include "ui/toast_view.h"

namespace snapper {

class ToastTests final : public QObject {
  Q_OBJECT

 private slots:
  void CountTextFollowsTheCount();
  void NewestStacksLastAndOldestLeavesWhenFull();
  void RepeatedProblemRefreshesInsteadOfStacking();
  void ToastsRunOutAfterTheirTime();
  void DismissTakesOneOrTheNewest();
  void UndoOnlyForExactlyTheAnnouncedStep();
  void ProblemIgnoresEmptyText();
  void DoneUndoesItsOwnStepAndNamesIt();
  void ANewerStepTakesTheUndoAway();
  void FinishShowsAProblemOrTheDoneToast();
  void WindowShowsToastsAndNeverTakesFocus();
  void ClickAndEscapeDismiss();
  void PressingUndoOnAToastUndoes();
  void DeletingAShotOffersUndoForThatShot();
};

namespace {

// Makes one more history step and returns its label.
QString MakeStep(Bench* rig, const QString& name) {
  assert(rig != nullptr);
  assert(!name.isEmpty());
  const bool is_made = rig->document.Rename(name).has_value();
  return is_made ? rig->history.UndoLabel() : QString();
}

}  // namespace

void ToastTests::CountTextFollowsTheCount() {
  const QString one = CountText("Deleted", 1, "key", "keys");
  const QString many = CountText("Deleted", 3, "key", "keys");
  QCOMPARE(one, QString("Deleted 1 key"));
  QCOMPARE(many, QString("Deleted 3 keys"));
}

void ToastTests::NewestStacksLastAndOldestLeavesWhenFull() {
  ToastQueue queue;
  for (int i = 0; i < kMaxToasts + 2; ++i) {
    queue.Push(QString("problem %1").arg(i));
  }
  QCOMPARE(static_cast<int>(queue.toasts().size()), kMaxToasts);
  QCOMPARE(queue.toasts().front().text, QString("problem 2"));
  QCOMPARE(queue.toasts().back().text, QString("problem 4"));
}

void ToastTests::RepeatedProblemRefreshesInsteadOfStacking() {
  ToastQueue queue;
  const int first = queue.Push("Nope");
  queue.Tick(3000);
  const int again = queue.Push("Nope");
  QCOMPARE(again, first);
  QCOMPARE(static_cast<int>(queue.toasts().size()), 1);
  QCOMPARE(queue.toasts().front().left_ms, kToastMs);
}

void ToastTests::ToastsRunOutAfterTheirTime() {
  ToastQueue queue;
  queue.Push("first");
  queue.Tick(kToastMs - 1000);
  queue.Push("second");
  QVERIFY(!queue.Tick(999));
  QVERIFY(queue.Tick(1));
  QCOMPARE(static_cast<int>(queue.toasts().size()), 1);
  QCOMPARE(queue.toasts().front().text, QString("second"));
  QVERIFY(queue.Tick(kToastMs));
  QVERIFY(queue.empty());
}

void ToastTests::DismissTakesOneOrTheNewest() {
  ToastQueue queue;
  const int a = queue.Push("a");
  queue.Push("b");
  queue.Push("c");
  QVERIFY(queue.Dismiss(a));
  QVERIFY(!queue.Dismiss(a));
  QVERIFY(queue.DismissNewest());
  QCOMPARE(queue.toasts().front().text, QString("b"));
  QVERIFY(queue.DismissNewest());
  QVERIFY(!queue.DismissNewest());
}

void ToastTests::UndoOnlyForExactlyTheAnnouncedStep() {
  ToastQueue queue;
  const int id = queue.PushUndoable("Deleted 2 keys", 7, "Delete keys");
  QVERIFY(!queue.TakeUndo(id, 8));
  QVERIFY(queue.Find(id) != nullptr);
  QVERIFY(queue.Retire(8));
  QVERIFY(!queue.Find(id)->has_undo);
  QVERIFY(!queue.TakeUndo(id, 7));
  const int other = queue.PushUndoable("Deleted 1 shot", 9, "Delete shot");
  QVERIFY(!queue.Retire(9));
  QVERIFY(queue.TakeUndo(other, 9));
  QVERIFY(queue.Find(other) == nullptr);
}

void ToastTests::ProblemIgnoresEmptyText() {
  Bench rig;
  QSignalSpy spy(&rig.toasts, &ToastManager::Changed);
  rig.toasts.Problem(QString());
  QCOMPARE(spy.count(), 0);
  QVERIFY(rig.toasts.queue().empty());
  rig.toasts.Problem("Nothing is picked.");
  QCOMPARE(spy.count(), 1);
  QCOMPARE(rig.toasts.queue().toasts().front().text,
           QString("Nothing is picked."));
}

void ToastTests::DoneUndoesItsOwnStepAndNamesIt() {
  Bench rig;
  MakeStep(&rig, "Before");
  const QString label = MakeStep(&rig, "After");
  QVERIFY(!label.isEmpty());
  rig.toasts.Done("Deleted 1 shot");
  const Toast toast = rig.toasts.queue().toasts().front();
  QVERIFY(toast.has_undo);
  QCOMPARE(toast.undo_label, label);
  rig.toasts.Undo(toast.id);
  QCOMPARE(rig.history.current().name, QString("Before"));
  QVERIFY(rig.toasts.queue().empty());
}

void ToastTests::ANewerStepTakesTheUndoAway() {
  Bench rig;
  MakeStep(&rig, "One");
  rig.toasts.Done("Deleted 1 key");
  const int id = rig.toasts.queue().toasts().front().id;
  MakeStep(&rig, "Two");
  QVERIFY(!rig.toasts.queue().toasts().front().has_undo);
  rig.toasts.Undo(id);
  QCOMPARE(rig.history.current().name, QString("Two"));
}

void ToastTests::FinishShowsAProblemOrTheDoneToast() {
  Bench rig;
  MakeStep(&rig, "One");
  rig.toasts.Finish("Nothing is picked.", "Deleted 1 key");
  QCOMPARE(rig.toasts.queue().toasts().back().text,
           QString("Nothing is picked."));
  QVERIFY(!rig.toasts.queue().toasts().back().has_undo);
  // A refusal is a red note, the news after it a yellow one.
  QVERIFY(rig.toasts.queue().toasts().back().kind == ToastKind::kProblem);
  rig.toasts.Finish(QString(), "Deleted 1 key");
  QVERIFY(rig.toasts.queue().toasts().back().has_undo);
  QVERIFY(rig.toasts.queue().toasts().back().kind == ToastKind::kInfo);
  rig.toasts.Cheer("Saved");
  QVERIFY(rig.toasts.queue().toasts().back().kind == ToastKind::kCheer);
}

void ToastTests::WindowShowsToastsAndNeverTakesFocus() {
  Bench rig;
  MainWindow window(rig.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  QWidget* before = window.focusWidget();
  rig.motion.SetMode(MotionMode::kOff);
  MakeStep(&rig, "One");
  rig.toasts.Done("Deleted 1 key");
  const auto items = window.findChildren<ToastItem*>();
  QCOMPARE(items.size(), 1);
  QVERIFY(items.first()->isVisible());
  QCOMPARE(items.first()->focusPolicy(), Qt::NoFocus);
  QCOMPARE(window.focusWidget(), before);
  const auto buttons = items.first()->findChildren<QPushButton*>();
  QCOMPARE(buttons.first()->focusPolicy(), Qt::NoFocus);
}

void ToastTests::ClickAndEscapeDismiss() {
  Bench rig;
  MainWindow window(rig.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  rig.motion.SetMode(MotionMode::kOff);
  rig.toasts.Problem("First");
  rig.toasts.Problem("Second");
  QTest::keyClick(&window, Qt::Key_Escape);
  QCOMPARE(static_cast<int>(rig.toasts.queue().toasts().size()), 1);
  QCOMPARE(rig.toasts.queue().toasts().front().text, QString("First"));
  auto* item = window.findChildren<ToastItem*>().first();
  QTest::mouseClick(item, Qt::LeftButton);
  QVERIFY(rig.toasts.queue().empty());
  QTRY_VERIFY(window.findChildren<ToastItem*>().isEmpty());
}

void ToastTests::PressingUndoOnAToastUndoes() {
  Bench rig;
  MainWindow window(rig.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  rig.motion.SetMode(MotionMode::kOff);
  MakeStep(&rig, "Kept");
  MakeStep(&rig, "Gone");
  rig.toasts.Done("Deleted 1 key");
  auto* item = window.findChildren<ToastItem*>().first();
  auto* undo = item->findChild<QPushButton*>();
  QVERIFY(undo->isVisible());
  QTest::mouseClick(undo, Qt::LeftButton);
  QCOMPARE(rig.history.current().name, QString("Kept"));
  QVERIFY(rig.toasts.queue().empty());
}

void ToastTests::DeletingAShotOffersUndoForThatShot() {
  Bench rig;
  QVERIFY(rig.shots.Add(-1).has_value());
  QVERIFY(rig.shots.Add(-1).has_value());
  ShotStrip strip(rig.All());
  strip.resize(800, 40);
  QTest::mouseClick(&strip, Qt::LeftButton, {},
                    strip.Blocks()[1].rect.center().toPoint());
  QTest::keyClick(&strip, Qt::Key_Delete);
  QCOMPARE(static_cast<int>(rig.history.current().shots.size()), 1);
  const Toast toast = rig.toasts.queue().toasts().back();
  QCOMPARE(toast.text, QString("Deleted 1 shot"));
  QVERIFY(toast.has_undo);
  rig.toasts.Undo(toast.id);
  QCOMPARE(static_cast<int>(rig.history.current().shots.size()), 2);
}

}  // namespace snapper

QTEST_MAIN(snapper::ToastTests)
#include "toast_tests.moc"
