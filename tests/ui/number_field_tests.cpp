#include <QApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QTest>

#include <cassert>

#include "bench.h"
#include "inspector_stage.h"
#include "ui/keyed_boxes.h"
#include "ui/new_project_dialog.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Clicks into the field like a hand would, then types and presses Enter
// on whatever has the keyboard, the way the real window routes keys.
void ClickAndType(QAbstractSpinBox* box, const QString& text) {
  assert(box != nullptr);
  assert(!text.isEmpty());
  QLineEdit* edit = box->findChild<QLineEdit*>();
  box->window()->show();
  QVERIFY2(QTest::qWaitForWindowExposed(box->window()), "not shown");
  QTest::mouseClick(edit, Qt::LeftButton);
  QVERIFY(QTest::qWaitFor([box] { return box->hasFocus(); }));
  edit->selectAll();
  QWidget* typing = QApplication::focusWidget();
  QVERIFY(typing != nullptr);
  QTest::keyClicks(typing, text);
  QTest::keyClick(typing, Qt::Key_Return);
}

}  // namespace

class NumberFieldTests final : public QObject {
  Q_OBJECT

 private slots:
  void PoseFieldTakesMathOnEnter();
  void DialogFieldsTakeMathOnEnter();
  void BadTextKeepsTheValue();
  void LeavingTheFieldAppliesTheMath();
};

void NumberFieldTests::PoseFieldTakesMathOnEnter() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  auto* turn = box.findChild<QDoubleSpinBox*>("turn");
  turn->setValue(10.0);
  ClickAndType(turn, "*2");
  QCOMPARE(turn->value(), 20.0);
  ClickAndType(turn, "+15");
  QCOMPARE(turn->value(), 35.0);
  ClickAndType(turn, "90/2");
  QCOMPARE(turn->value(), 45.0);
}

void NumberFieldTests::DialogFieldsTakeMathOnEnter() {
  NewProjectDialog dialog(nullptr);
  auto* width = dialog.findChildren<QSpinBox*>().first();
  width->setValue(400);
  ClickAndType(width, "*2");
  QCOMPARE(width->value(), 800);
  ClickAndType(width, "+100");
  QCOMPARE(width->value(), 900);
}

void NumberFieldTests::BadTextKeepsTheValue() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  auto* turn = box.findChild<QDoubleSpinBox*>("turn");
  turn->setValue(10.0);
  ClickAndType(turn, "banana");
  QCOMPARE(turn->value(), 10.0);
  QCOMPARE(turn->text(), QString("10.0 deg"));
  QCOMPARE(turn->findChild<QLineEdit*>()->palette().color(QPalette::Base),
           theme::kFieldBad);
}

void NumberFieldTests::LeavingTheFieldAppliesTheMath() {
  Bench bench;
  Stage(&bench);
  PoseBox box(bench.All());
  auto* turn = box.findChild<QDoubleSpinBox*>("turn");
  turn->setValue(10.0);
  box.show();
  turn->setFocus();
  QVERIFY(QTest::qWaitFor([turn] { return turn->hasFocus(); }));
  QLineEdit* edit = turn->findChild<QLineEdit*>();
  edit->selectAll();
  QTest::keyClicks(edit, "*3");
  box.findChild<QDoubleSpinBox*>("x")->setFocus();
  QVERIFY(QTest::qWaitFor([turn] { return !turn->hasFocus(); }));
  QCOMPARE(turn->value(), 30.0);
}

}  // namespace snapper

QTEST_MAIN(snapper::NumberFieldTests)
#include "number_field_tests.moc"
