#include <QTest>

#include <cassert>
#include <cmath>
#include <vector>

#include "base/number_text.h"
#include "base/scrub_drag.h"

namespace snapper {

class NumberTests final : public QObject {
  Q_OBJECT

 private slots:
  void PlainNumbersAndSums();
  void LeadingOperatorsWorkOnTheCurrentValue();
  void BracketsAndSignsFollowMathRules();
  void BadTextGivesNothing();
  void MixedMeansNotAllTheSame();
  void ClicksInTheDeadZoneChangeNothing();
  void PastTheDeadZoneTheValueJumpsToTheCursor();
  void ShiftAndAltScaleThePixels();
  void ModifierMidDragNeverJumps();
  void ValuesStayInRangeAndRound();
};

namespace {

double Eval(const QString& text, double current = 12.0) {
  assert(std::isfinite(current));
  assert(text.size() < 1000);
  const auto value = EvalNumber(text, current);
  return value.has_value() ? *value : -999999.0;
}

ScrubDrag Drag(double start) {
  assert(std::isfinite(start));
  assert(start >= -100.0 && start <= 100.0);
  return ScrubDrag(start, ScrubRange{1.0, 1, -100.0, 100.0});
}

}  // namespace

void NumberTests::PlainNumbersAndSums() {
  QCOMPARE(Eval("45"), 45.0);
  QCOMPARE(Eval("-5"), -5.0);
  QCOMPARE(Eval("90/2"), 45.0);
  QCOMPARE(Eval("2.5*4"), 10.0);
  QCOMPARE(Eval("1,5+1"), 2.5);
  QCOMPARE(Eval(" 10 - 3 - 2 "), 5.0);
  QCOMPARE(Eval("2+3*4"), 14.0);
}

void NumberTests::LeadingOperatorsWorkOnTheCurrentValue() {
  QCOMPARE(Eval("*2"), 24.0);
  QCOMPARE(Eval("+15"), 27.0);
  QCOMPARE(Eval("/4"), 3.0);
  QCOMPARE(Eval("*2+1", 10.0), 21.0);
  QCOMPARE(Eval("+-5"), 7.0);
}

void NumberTests::BracketsAndSignsFollowMathRules() {
  QCOMPARE(Eval("(2+3)*4"), 20.0);
  QCOMPARE(Eval("2*-3"), -6.0);
  QCOMPARE(Eval("--4"), 4.0);
  QCOMPARE(Eval("-(1+2)"), -3.0);
  QCOMPARE(Eval("-2*3"), -6.0);
}

void NumberTests::BadTextGivesNothing() {
  QVERIFY(Eval("1/0") < -999990.0);
  for (const QString& bad :
       {QString(), QString("abc"), QString("*"), QString("1/0"),
        QString("2**3"), QString("(1+2"), QString("1+2)"), QString("1..2"),
        QString("*2*"), QString("/0"), QString(")("),
        QString(65, QLatin1Char('1'))}) {
    QVERIFY2(!EvalNumber(bad, 12.0).has_value(), qPrintable(bad));
  }
}

void NumberTests::MixedMeansNotAllTheSame() {
  QVERIFY(!HasMixedValues({}));
  QVERIFY(!HasMixedValues({3.0}));
  QVERIFY(!HasMixedValues({3.0, 3.0, 3.0}));
  QVERIFY(HasMixedValues({3.0, 3.0, 4.0}));
}

void NumberTests::ClicksInTheDeadZoneChangeNothing() {
  ScrubDrag drag = Drag(10.0);
  QCOMPARE(drag.Move(3, {}), 10.0);
  QCOMPARE(drag.Move(-3, {}), 10.0);
  QVERIFY(!drag.is_active());
}

void NumberTests::PastTheDeadZoneTheValueJumpsToTheCursor() {
  ScrubDrag drag = Drag(10.0);
  QCOMPARE(drag.Move(4, {}), 14.0);
  QVERIFY(drag.is_active());
  QCOMPARE(drag.Move(9, {}), 19.0);
  // Back inside the dead zone it keeps following: it is a drag now.
  QCOMPARE(drag.Move(1, {}), 11.0);
  QCOMPARE(drag.Move(-6, {}), 4.0);
}

void NumberTests::ShiftAndAltScaleThePixels() {
  ScrubDrag fast = Drag(0.0);
  QCOMPARE(fast.Move(5, {true, false}), 50.0);
  ScrubDrag slow = Drag(0.0);
  QCOMPARE(slow.Move(5, {false, true}), 0.5);
  ScrubDrag both = Drag(0.0);
  QCOMPARE(both.Move(5, {true, true}), 5.0);
}

void NumberTests::ModifierMidDragNeverJumps() {
  ScrubDrag drag = Drag(0.0);
  QCOMPARE(drag.Move(5, {}), 5.0);
  QCOMPARE(drag.Move(5, {true, false}), 5.0);
  QCOMPARE(drag.Move(6, {true, false}), 15.0);
  QCOMPARE(drag.Move(7, {}), 16.0);
}

void NumberTests::ValuesStayInRangeAndRound() {
  ScrubDrag drag = Drag(95.0);
  QCOMPARE(drag.Move(20, {}), 100.0);
  // The clamp is part of the drag: coming back moves at once.
  QCOMPARE(drag.Move(19, {}), 99.0);
  ScrubDrag fine(0.0, ScrubRange{0.01, 2, 0.0, 1.0});
  QCOMPARE(fine.Move(4, {false, true}), 0.0);
  QCOMPARE(fine.Move(14, {false, true}), 0.01);
  ScrubDrag whole(0.0, ScrubRange{2.0, 0, 0.0, 100.0});
  QCOMPARE(whole.Move(5, {}), 10.0);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::NumberTests)
#include "number_tests.moc"
