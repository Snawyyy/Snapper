#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSpinBox>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "inspector_stage.h"
#include "ui/keyed_boxes.h"
#include "ui/new_project_dialog.h"
#include "ui/shot_box.h"
#include "ui/scrub_label.h"

namespace snapper {
namespace {

// The scrubbing name beside a field of box.
ScrubLabel* LabelOf(PoseBox* box, const QString& text) {
  assert(box != nullptr);
  assert(!text.isEmpty());
  for (ScrubLabel* label : box->findChildren<ScrubLabel*>()) {
    const bool is_it = label->text() == text;
    if (is_it) {
      return label;
    }
  }
  return nullptr;
}

void Send(ScrubLabel* label, QEvent::Type type, int x,
          Qt::KeyboardModifiers keys = Qt::NoModifier) {
  assert(label != nullptr);
  assert(type == QEvent::MouseButtonPress || type == QEvent::MouseMove ||
         type == QEvent::MouseButtonRelease);
  const bool is_move = type == QEvent::MouseMove;
  const Qt::MouseButton button = is_move ? Qt::NoButton : Qt::LeftButton;
  const Qt::MouseButtons held =
      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
  QMouseEvent event(type, QPointF(5, 5), QPointF(x, 100), button, held,
                    keys);
  QApplication::sendEvent(label, &event);
}

void PressEscape(ScrubLabel* label) {
  assert(label != nullptr);
  assert(label->is_dragging());
  QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QApplication::sendEvent(label, &event);
}

void TypeInto(QDoubleSpinBox* box, const QString& text) {
  assert(box != nullptr);
  assert(!text.isEmpty());
  QLineEdit* edit = box->findChild<QLineEdit*>();
  assert(edit != nullptr);
  // Typed in, so it has focus, as it does in the app.
  box->window()->show();
  box->setFocus();
  const bool has_focus = QTest::qWaitFor([box] { return box->hasFocus(); });
  assert(has_focus);
  edit->selectAll();
  QTest::keyClicks(edit, text);
  QTest::keyClick(edit, Qt::Key_Return);
}

}  // namespace

class ScrubTests final : public QObject {
  Q_OBJECT

 private slots:
  void DraggingTheLabelChangesTheFieldAsOneUndoStep();
  void ACloseClickChangesNothing();
  void EscapePutsTheValueBack();
  void ShiftAndAltChangeTheSpeed();
  void MathAppliesOnEnter();
  void BadMathKeepsTheValue();
  void FieldsAreNotRewrittenWhileTyped();
  void PicksThatDifferShowMixed();
  void ALabelGreysWithItsField();
  void ShotSecondsStepHalfAndRoundToTheFrame();
  void ShotLengthsThatDifferShowMixed();
  void NewProjectSizesStepAndScrubByTwo();
};

void ScrubTests::DraggingTheLabelChangesTheFieldAsOneUndoStep() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  ScrubLabel* label = LabelOf(&box, "Turn");
  QVERIFY(label != nullptr);
  QCOMPARE(label->cursor().shape(), Qt::SizeHorCursor);
  Send(label, QEvent::MouseButtonPress, 100);
  Send(label, QEvent::MouseMove, 112);
  Send(label, QEvent::MouseMove, 130);
  QVERIFY(label->is_dragging());
  QVERIFY(bench.history.IsScopeOpen());
  QCOMPARE(box.findChild<QDoubleSpinBox*>("turn")->value(), 30.0);
  QCOMPARE(Head(bench, 0).rotation, 30.0);
  Send(label, QEvent::MouseButtonRelease, 130);
  QVERIFY(!bench.history.IsScopeOpen());
  QCOMPARE(bench.history.UndoLabel(), QString("Pose"));
  bench.history.Undo();
  QCOMPARE(Head(bench, 0).rotation, 0.0);
  QVERIFY(!bench.history.CanUndo());
  QCOMPARE(box.findChild<QDoubleSpinBox*>("turn")->value(), 0.0);
}

void ScrubTests::ACloseClickChangesNothing() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  ScrubLabel* label = LabelOf(&box, "Turn");
  Send(label, QEvent::MouseButtonPress, 100);
  Send(label, QEvent::MouseMove, 103);
  Send(label, QEvent::MouseButtonRelease, 103);
  QVERIFY(!bench.history.CanUndo());
  QVERIFY(!bench.history.IsScopeOpen());
  QVERIFY(!label->is_dragging());
}

void ScrubTests::EscapePutsTheValueBack() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  ScrubLabel* label = LabelOf(&box, "Turn");
  Send(label, QEvent::MouseButtonPress, 100);
  Send(label, QEvent::MouseMove, 140);
  QCOMPARE(Head(bench, 0).rotation, 40.0);
  PressEscape(label);
  QVERIFY(!label->is_dragging());
  QVERIFY(!bench.history.IsScopeOpen());
  QVERIFY(!bench.history.CanUndo());
  QCOMPARE(Head(bench, 0).rotation, 0.0);
  QCOMPARE(box.findChild<QDoubleSpinBox*>("turn")->value(), 0.0);
  // The release after Escape is not a second drag.
  Send(label, QEvent::MouseButtonRelease, 140);
  QVERIFY(!bench.history.CanUndo());
}

void ScrubTests::ShiftAndAltChangeTheSpeed() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  ScrubLabel* label = LabelOf(&box, "Turn");
  Send(label, QEvent::MouseButtonPress, 100);
  Send(label, QEvent::MouseMove, 105, Qt::ShiftModifier);
  QCOMPARE(Head(bench, 0).rotation, 50.0);
  Send(label, QEvent::MouseMove, 115, Qt::AltModifier);
  QCOMPARE(Head(bench, 0).rotation, 51.0);
  Send(label, QEvent::MouseButtonRelease, 115);
  QCOMPARE(bench.history.UndoLabel(), QString("Pose"));
}

void ScrubTests::MathAppliesOnEnter() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  auto* turn = box.findChild<QDoubleSpinBox*>("turn");
  turn->setValue(10.0);
  emit turn->editingFinished();
  TypeInto(turn, "*2");
  QCOMPARE(turn->value(), 20.0);
  QCOMPARE(Head(bench, 0).rotation, 20.0);
  TypeInto(turn, "+15");
  QCOMPARE(Head(bench, 0).rotation, 35.0);
  TypeInto(turn, "90/2");
  QCOMPARE(Head(bench, 0).rotation, 45.0);
  QCOMPARE(turn->text(), QString("45.0 deg"));
}

void ScrubTests::BadMathKeepsTheValue() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  auto* turn = box.findChild<QDoubleSpinBox*>("turn");
  turn->setValue(10.0);
  emit turn->editingFinished();
  TypeInto(turn, "1/0");
  QCOMPARE(turn->value(), 10.0);
  QCOMPARE(Head(bench, 0).rotation, 10.0);
  QCOMPARE(turn->text(), QString("10.0 deg"));
  TypeInto(turn, "banana");
  QCOMPARE(turn->value(), 10.0);
  QCOMPARE(Head(bench, 0).rotation, 10.0);
  bench.history.Undo();
  QCOMPARE(Head(bench, 0).rotation, 0.0);
  QVERIFY(!bench.history.CanUndo());
}

void ScrubTests::FieldsAreNotRewrittenWhileTyped() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  box.show();
  auto* turn = box.findChild<QDoubleSpinBox*>("turn");
  turn->setFocus();
  QVERIFY(QTest::qWaitFor([turn] { return turn->hasFocus(); }));
  QLineEdit* edit = turn->findChild<QLineEdit*>();
  edit->selectAll();
  QTest::keyClicks(edit, "*");
  bench.selection.SelectPieces({"head"}, false);
  bench.playback.Seek(Frame(3));
  QCOMPARE(edit->text(), QString("*"));
}

void ScrubTests::PicksThatDifferShowMixed() {
  Bench bench;
  Stage(&bench);
  Project project = bench.history.current();
  Doll doll = *project.dolls.at("Bob");
  doll.rig.pieces.push_back({"arm", "", {}, 0, -1, {}});
  project.dolls["Bob"] = std::make_shared<const Doll>(doll);
  bench.history.Reset(project);
  const TrackRef arm{kShot, TrackKind::kPiece, LayerId(1), "arm"};
  QVERIFY(bench.pose.Move(arm, Frame(0), QPointF(10, 0)).has_value());
  bench.selection.SelectLayer(LayerId(1));
  bench.selection.PickThings({{LayerId(1), "head"}, {LayerId(1), "arm"}},
                             PickMode::kReplace, LayerId(1));
  PoseBox box(bench.All());
  const QString mixed = QObject::tr("Mixed");
  QCOMPARE(box.findChild<QDoubleSpinBox*>("x")->text(), mixed);
  // Turn is the same on both, so it shows its number.
  QCOMPARE(box.findChild<QDoubleSpinBox*>("turn")->text(),
           QString("0.0 deg"));
  bench.selection.PickThings({{LayerId(1), "head"}}, PickMode::kReplace,
                             LayerId(1));
  QCOMPARE(box.findChild<QDoubleSpinBox*>("x")->text(), QString("0.0 px"));
}

void ScrubTests::ALabelGreysWithItsField() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  ScrubLabel* label = LabelOf(&box, "Lean");
  QVERIFY(label != nullptr);
  // Lean needs a whole doll picked; for a single piece it is hidden.
  QVERIFY(!label->isVisibleTo(&box));
  bench.selection.SelectLayer(LayerId(1));
  QVERIFY(label->isVisibleTo(&box));
  QVERIFY(label->isEnabled());
}

void ScrubTests::ShotSecondsStepHalfAndRoundToTheFrame() {
  Bench bench;
  Stage(&bench);
  ShotBox box(bench.All());
  auto* seconds = box.findChild<QDoubleSpinBox*>("shot_seconds");
  QCOMPARE(seconds->singleStep(), 0.5);
  const double before = seconds->value();
  seconds->stepUp();
  emit seconds->editingFinished();
  QCOMPARE(seconds->value(), before + 0.5);
  QCOMPARE(bench.history.current().shots[0]->length,
           FramesNearSeconds(before + 0.5));
  TypeInto(seconds, "2.53");
  QCOMPARE(bench.history.current().shots[0]->length, Frame(61));
}

void ScrubTests::ShotLengthsThatDifferShowMixed() {
  Bench bench;
  Stage(&bench);
  Project project = bench.history.current();
  project.next_shot_id = 2;
  bench.history.Reset(project);
  QVERIFY(bench.shots.Add(-1).has_value());
  const ShotId second = bench.history.current().shots[1]->id;
  QVERIFY(bench.shots.SetLength(second, Frame(30)).has_value());
  bench.selection.PickShots({kShot, second}, PickMode::kReplace);
  ShotBox box(bench.All());
  QCOMPARE(box.findChild<QSpinBox*>("shot_length")->text(),
           QObject::tr("Mixed"));
  bench.selection.SelectShot(second);
  QCOMPARE(box.findChild<QSpinBox*>("shot_length")->text(),
           QString("30 frames"));
}

void ScrubTests::NewProjectSizesStepAndScrubByTwo() {
  NewProjectDialog dialog(nullptr);
  auto* width = dialog.findChildren<QSpinBox*>().first();
  QCOMPARE(width->singleStep(), 2);
  QCOMPARE(width->property("scrub_unit").toDouble(), 2.0);
  const int before = width->value();
  width->stepUp();
  QCOMPARE(width->value(), before + 2);
  QVERIFY(dialog.WhyNotReady().isEmpty());
}

}  // namespace snapper

QTEST_MAIN(snapper::ScrubTests)
#include "scrub_tests.moc"
