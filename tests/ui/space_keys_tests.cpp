#include <QApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "edit/timeline_rows.h"
#include "ui/main_window.h"
#include "ui/number_input.h"
#include "ui/space_keys_action.h"
#include "ui/toast_queue.h"

namespace snapper {
namespace {

const ShotId kShot(1);

// One character layer with pose keys at frames 2, 4 and 6.
void Stage(Bench* bench) {
  assert(bench != nullptr);
  Layer layer;
  layer.id = LayerId(1);
  layer.name = "Bob";
  layer.content = CharacterLayer{"Bob", {}, false};
  Shot shot;
  shot.id = kShot;
  shot.layers = {layer};
  Project project;
  project.shots = {std::make_shared<const Shot>(shot)};
  bench->history.Reset(project);
  for (const int frame : {2, 4, 6}) {
    QVERIFY(bench->pose.KeyInPlace(kShot, LayerId(1), Frame(frame))
                .has_value());
  }
}

std::vector<int> BobFrames(const Bench& bench) {
  assert(!bench.history.current().shots.empty());
  std::vector<int> frames;
  const auto rows = TimelineRows(bench.history.current(), kShot);
  assert(!rows.empty());
  for (const Frame frame : rows[0].keys) {
    frames.push_back(frame.index());
  }
  return frames;
}

// Picks every key on Bob's row.
void PickAll(Bench* bench) {
  assert(bench != nullptr);
  const Project& project = bench->history.current();
  std::set<KeyRef> all;
  const auto rows = TimelineRows(project, kShot);
  assert(!rows.empty());
  for (const Frame frame : rows[0].keys) {
    all.merge(RowKeysAt(project, rows[0], frame));
  }
  bench->selection.PickKeys(all, PickMode::kReplace);
}

}  // namespace

class SpaceKeysTests final : public QObject {
  Q_OBJECT

 private slots:
  void DigitsMapToSteps();
  void ThreeSpacesPickedKeysInOneUndoStep();
  void KeysStayPickedAfterwards();
  void NothingPickedToastsTheReason();
  void FiveDoesNothing();
  void DigitInANumberFieldTypes();
};

void SpaceKeysTests::DigitsMapToSteps() {
  QCOMPARE(SpaceStepOfKey(Qt::Key_1), 1);
  QCOMPARE(SpaceStepOfKey(Qt::Key_4), 4);
  QCOMPARE(SpaceStepOfKey(Qt::Key_0), 0);
  QCOMPARE(SpaceStepOfKey(Qt::Key_5), 0);
  QCOMPARE(SpaceStepOfKey(Qt::Key_A), 0);
  QVERIFY(SpaceShortcut(3) == QKeySequence(Qt::Key_3));
  QVERIFY(SpaceShortcut(5).isEmpty());
}

void SpaceKeysTests::ThreeSpacesPickedKeysInOneUndoStep() {
  Bench bench;
  Stage(&bench);
  MainWindow window(bench.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  PickAll(&bench);
  QTest::keyClick(&window, Qt::Key_3);
  QCOMPARE(BobFrames(bench), std::vector<int>({2, 5, 8}));
  QCOMPARE(bench.history.UndoLabel(), QString("Space keys on 3s"));
  bench.history.Undo();
  QCOMPARE(BobFrames(bench), std::vector<int>({2, 4, 6}));
}

void SpaceKeysTests::KeysStayPickedAfterwards() {
  Bench bench;
  Stage(&bench);
  PickAll(&bench);
  const size_t picked = bench.selection.keys().size();
  RequestSpaceKeys(bench.All(), 3);
  QCOMPARE(bench.selection.keys().size(), picked);
  QCOMPARE(bench.selection.keys().rbegin()->frame, Frame(8));
}

void SpaceKeysTests::NothingPickedToastsTheReason() {
  Bench bench;
  Stage(&bench);
  MainWindow window(bench.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  const QString before = bench.history.UndoLabel();
  QTest::keyClick(&window, Qt::Key_2);
  QCOMPARE(bench.history.UndoLabel(), before);
  QCOMPARE(BobFrames(bench), std::vector<int>({2, 4, 6}));
  QCOMPARE(bench.toasts.queue().toasts().size(), size_t{1});
  QCOMPARE(bench.toasts.queue().toasts()[0].text,
           bench.keys.WhyNoSpace({}));
}

void SpaceKeysTests::FiveDoesNothing() {
  Bench bench;
  Stage(&bench);
  MainWindow window(bench.All());
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  PickAll(&bench);
  QTest::keyClick(&window, Qt::Key_5);
  QCOMPARE(BobFrames(bench), std::vector<int>({2, 4, 6}));
  QVERIFY(bench.toasts.queue().toasts().empty());
}

void SpaceKeysTests::DigitInANumberFieldTypes() {
  Bench bench;
  Stage(&bench);
  MainWindow window(bench.All());
  QDoubleSpinBox box(&window);
  NumberInput::Install(&box);
  QLineEdit* edit = box.findChild<QLineEdit*>();
  QVERIFY(edit != nullptr);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window));
  PickAll(&bench);
  box.setFocus();
  QVERIFY(QTest::qWaitFor([&box] { return box.hasFocus(); }));
  edit->selectAll();
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_3);
  QVERIFY(edit->text().contains('3'));
  QCOMPARE(BobFrames(bench), std::vector<int>({2, 4, 6}));
}

}  // namespace snapper

QTEST_MAIN(snapper::SpaceKeysTests)
#include "space_keys_tests.moc"
