#include <QTest>

#include <cassert>
#include <vector>

#include "edit/change_watcher.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"

namespace snapper {
namespace {

Project Named(const QString& name) {
  assert(!name.isEmpty());
  assert(name.size() < 100);
  Project project;
  project.name = name;
  return project;
}

// What one Stepped said: the name of the project before, and the kind.
struct Heard final {
  QString before;
  bool is_travel;

  bool operator==(const Heard&) const = default;
};

// Collects every step a watcher announces.
struct Listener final {
  Listener(HistoryManager* history, QObject* owner)
      : watcher(history, owner) {
    assert(history != nullptr);
    assert(owner != nullptr);
    QObject::connect(&watcher, &ChangeWatcher::Stepped, owner,
                     [this](const Project& before, bool is_travel) {
                       heard.push_back({before.name, is_travel});
                     });
  }
  ChangeWatcher watcher;
  std::vector<Heard> heard;
};

}  // namespace

class ChangeWatcherTests final : public QObject {
  Q_OBJECT

 private slots:
  void AnEditHearsTheProjectBeforeIt();
  void UndoAndRedoAreTravel();
  void AnEditAfterUndoIsNotTravel();
  void ADragIsHeardOnceWhenItLands();
  void ACancelledDragIsNotHeard();
  void OpeningAFileIsNotHeard();
  void SavingIsNotHeard();
  void TheNextEditAfterAnOpenStartsFromIt();
};

void ChangeWatcherTests::AnEditHearsTheProjectBeforeIt() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  history.Commit("Rename", Named("b"));
  history.Commit("Rename", Named("c"));
  QCOMPARE(listener.heard, (std::vector<Heard>{{"a", false}, {"b", false}}));
  QCOMPARE(history.current().name, QString("c"));
}

void ChangeWatcherTests::UndoAndRedoAreTravel() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  history.Commit("Rename", Named("b"));
  history.Undo();
  history.Redo();
  QCOMPARE(listener.heard, (std::vector<Heard>{
                               {"a", false}, {"b", true}, {"a", true}}));
  QCOMPARE(history.current().name, QString("b"));
}

void ChangeWatcherTests::AnEditAfterUndoIsNotTravel() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  history.Commit("Rename", Named("b"));
  history.Undo();
  history.Commit("Rename", Named("c"));
  QCOMPARE(listener.heard.back(), (Heard{"a", false}));
  QCOMPARE(listener.heard.size(), size_t{3});
}

void ChangeWatcherTests::ADragIsHeardOnceWhenItLands() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  {
    EditScope scope(&history, "Drag");
    scope.Preview(Named("b"));
    scope.Preview(Named("c"));
    QVERIFY(listener.heard.empty());
  }
  QCOMPARE(listener.heard, (std::vector<Heard>{{"a", false}}));
  QCOMPARE(history.current().name, QString("c"));
}

void ChangeWatcherTests::ACancelledDragIsNotHeard() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  {
    EditScope scope(&history, "Drag");
    scope.Preview(Named("b"));
    scope.Cancel();
  }
  QVERIFY(listener.heard.empty());
  QCOMPARE(history.current().name, QString("a"));
}

void ChangeWatcherTests::OpeningAFileIsNotHeard() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  history.Commit("Rename", Named("b"));
  history.Reset(Named("file"));
  QCOMPARE(listener.heard.size(), size_t{1});
  QCOMPARE(history.current().name, QString("file"));
}

void ChangeWatcherTests::SavingIsNotHeard() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  history.MarkSaved();
  QVERIFY(listener.heard.empty());
  QVERIFY(!history.IsDirty());
}

void ChangeWatcherTests::TheNextEditAfterAnOpenStartsFromIt() {
  HistoryManager history(Named("a"));
  Listener listener(&history, this);
  history.Reset(Named("file"));
  history.Commit("Rename", Named("b"));
  QCOMPARE(listener.heard, (std::vector<Heard>{{"file", false}}));
  QVERIFY(history.CanUndo());
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::ChangeWatcherTests)
#include "change_watcher_tests.moc"
