#include <QSignalSpy>
#include <QTest>

#include <optional>

#include "edit/edit_scope.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

Project Named(const QString& name) {
  Project project;
  project.name = name;
  return project;
}

}  // namespace

class HistoryTests final : public QObject {
  Q_OBJECT

 private slots:
  void StartsCleanWithNothingToUndo();
  void CommitUndoRedoWalkTheSteps();
  void UnchangedCommitRecordsNothing();
  void NewEditClearsRedo();
  void UndoBackToSaveIsClean();
  void HistoryDropsTheOldestPastTheLimit();
  void DragLandsAsOneStep();
  void CancelledDragPutsTheStartBack();
  void UndoIsBlockedDuringADrag();
  void ResetForgetsHistory();
};

void HistoryTests::StartsCleanWithNothingToUndo() {
  const HistoryManager history(Named("a"));
  QCOMPARE(history.current().name, QString("a"));
  QVERIFY(!history.IsDirty());
  QVERIFY(!history.CanUndo());
  QVERIFY(!history.CanRedo());
  QCOMPARE(history.WhyNoUndo(), QString("Nothing to undo"));
  QVERIFY(history.UndoLabel().isEmpty());
}

void HistoryTests::CommitUndoRedoWalkTheSteps() {
  HistoryManager history(Named("a"));
  QSignalSpy changed(&history, &HistoryManager::Changed);
  history.Commit("Rename", Named("b"));
  QCOMPARE(history.current().name, QString("b"));
  QCOMPARE(history.UndoLabel(), QString("Rename"));
  QVERIFY(history.WhyNoUndo().isEmpty());
  history.Undo();
  QCOMPARE(history.current().name, QString("a"));
  QCOMPARE(history.RedoLabel(), QString("Rename"));
  history.Redo();
  QCOMPARE(history.current().name, QString("b"));
  QCOMPARE(changed.count(), 3);
}

void HistoryTests::UnchangedCommitRecordsNothing() {
  HistoryManager history(Named("a"));
  QSignalSpy changed(&history, &HistoryManager::Changed);
  history.Commit("Nothing", Named("a"));
  QVERIFY(!history.CanUndo());
  QVERIFY(!history.IsDirty());
  QCOMPARE(changed.count(), 0);
}

void HistoryTests::NewEditClearsRedo() {
  HistoryManager history(Named("a"));
  history.Commit("To b", Named("b"));
  history.Undo();
  history.Commit("To c", Named("c"));
  QVERIFY(!history.CanRedo());
  history.Undo();
  QCOMPARE(history.current().name, QString("a"));
}

void HistoryTests::UndoBackToSaveIsClean() {
  HistoryManager history(Named("a"));
  history.Commit("To b", Named("b"));
  QVERIFY(history.IsDirty());
  history.MarkSaved();
  QVERIFY(!history.IsDirty());
  history.Commit("To c", Named("c"));
  QVERIFY(history.IsDirty());
  history.Undo();
  QVERIFY(!history.IsDirty());
  history.Undo();
  QVERIFY(history.IsDirty());
  history.Redo();
  QVERIFY(!history.IsDirty());
}

void HistoryTests::HistoryDropsTheOldestPastTheLimit() {
  HistoryManager history(Named("start"));
  for (int i = 1; i <= kMaxUndoSteps + 1; ++i) {
    history.Commit("Step", Named(QString::number(i)));
  }
  for (int i = 0; i < kMaxUndoSteps; ++i) {
    history.Undo();
  }
  QVERIFY(!history.CanUndo());
  // Step 1's "before" (start) was dropped; the oldest left is 1.
  QCOMPARE(history.current().name, QString("1"));
}

void HistoryTests::DragLandsAsOneStep() {
  HistoryManager history(Named("a"));
  {
    EditScope drag(&history, "Drag");
    drag.Preview(Named("b"));
    QCOMPARE(history.current().name, QString("b"));
    QVERIFY(history.IsDirty());
    drag.Preview(Named("c"));
  }
  QCOMPARE(history.current().name, QString("c"));
  QCOMPARE(history.UndoLabel(), QString("Drag"));
  history.Undo();
  QCOMPARE(history.current().name, QString("a"));
  QVERIFY(!history.IsDirty());
  QVERIFY(!history.CanUndo());
}

void HistoryTests::CancelledDragPutsTheStartBack() {
  HistoryManager history(Named("a"));
  {
    EditScope drag(&history, "Drag");
    drag.Preview(Named("b"));
    drag.Cancel();
  }
  QCOMPARE(history.current().name, QString("a"));
  QVERIFY(!history.CanUndo());
  QVERIFY(!history.IsDirty());
  {
    // A drag that ends where it started is not a step either.
    EditScope drag(&history, "Drag");
    drag.Preview(Named("a"));
  }
  QVERIFY(!history.CanUndo());
  QVERIFY(!history.IsDirty());
}

void HistoryTests::UndoIsBlockedDuringADrag() {
  HistoryManager history(Named("a"));
  history.Commit("To b", Named("b"));
  std::optional<EditScope> drag;
  drag.emplace(&history, "Drag");
  QVERIFY(!history.CanUndo());
  QCOMPARE(history.WhyNoUndo(), QString("Finish the drag first"));
  history.Undo();
  QCOMPARE(history.current().name, QString("b"));
  drag.reset();
  QVERIFY(history.CanUndo());
}

void HistoryTests::ResetForgetsHistory() {
  HistoryManager history(Named("a"));
  history.Commit("To b", Named("b"));
  history.Reset(Named("opened"));
  QCOMPARE(history.current().name, QString("opened"));
  QVERIFY(!history.CanUndo());
  QVERIFY(!history.IsDirty());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::HistoryTests)
#include "history_tests.moc"
