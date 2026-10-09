#include <QTest>

#include "ui/bar_text.h"

namespace snapper {

class BarTextTests final : public QObject {
  Q_OBJECT

 private slots:
  void StepNamesWhatItUndoes();
  void StepGreysOutWithAReason();
  void CounterPadsToThreeDigits();
  void TimecodeCountsFramesInTheSecond();
  void PlayButtonSaysWhatItWillDo();
};

void BarTextTests::StepNamesWhatItUndoes() {
  const StepFace face = MakeStepFace("Undo", "Rotate head", "");
  QVERIFY(face.enabled);
  QCOMPARE(face.text, QString("Undo Rotate head"));
  QVERIFY(face.tooltip.isEmpty());
}

void BarTextTests::StepGreysOutWithAReason() {
  const StepFace face = MakeStepFace("Redo", "", "Nothing to redo");
  QVERIFY(!face.enabled);
  QCOMPARE(face.text, QString("Redo"));
  QCOMPARE(face.tooltip, QString("Nothing to redo"));
}

void BarTextTests::CounterPadsToThreeDigits() {
  QCOMPARE(FrameCounterText(Frame(0)), QString("F 000"));
  QCOMPARE(FrameCounterText(Frame(24)), QString("F 024"));
  QCOMPARE(FrameCounterText(Frame(1234)), QString("F 1234"));
}

void BarTextTests::TimecodeCountsFramesInTheSecond() {
  QCOMPARE(TimecodeText(Frame(0)), QString("0:00.00"));
  QCOMPARE(TimecodeText(Frame(24)), QString("0:01.00"));
  QCOMPARE(TimecodeText(Frame(24 * 61 + 5)), QString("1:01.05"));
}

void BarTextTests::PlayButtonSaysWhatItWillDo() {
  QCOMPARE(PlayText(false), QString("Play"));
  QCOMPARE(PlayText(true), QString("Pause"));
}

}  // namespace snapper

QTEST_MAIN(snapper::BarTextTests)
#include "bar_text_tests.moc"
