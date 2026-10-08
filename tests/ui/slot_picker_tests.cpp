#include <QDialogButtonBox>
#include <QPushButton>
#include <QSlider>
#include <QTest>

#include "bench.h"
#include "ui/slot_picker.h"

namespace snapper {

class SlotPickerTests final : public QObject {
  Q_OBJECT

 private slots:
  void SliderSlidesTheVideoUnderTheGap();
  void ShortVideosCantFill();
};

void SlotPickerTests::SliderSlidesTheVideoUnderTheGap() {
  Bench bench;
  const Slot gap{Frame(24), Frame(72)};
  SlotPicker picker(bench.All(), gap,
                    {VideoSource{"/paint.mp4", Frame(240)}, Frame(30)});
  auto* start = picker.findChild<QSlider*>("slot_start");
  QCOMPARE(start->maximum(), 192);
  QCOMPARE(start->value(), 30);
  start->setValue(120);
  QCOMPARE(picker.in(), Frame(120));
  QVERIFY(std::holds_alternative<VideoSource>(picker.source()));
}

void SlotPickerTests::ShortVideosCantFill() {
  Bench bench;
  const Slot gap{Frame(0), Frame(48)};
  SlotPicker picker(bench.All(), gap,
                    {VideoSource{"/tiny.mp4", Frame(10)}, Frame(0)});
  auto* box = picker.findChild<QDialogButtonBox*>();
  QVERIFY(!box->button(QDialogButtonBox::Ok)->isEnabled());
  QVERIFY(!picker.findChild<QSlider*>("slot_start")->isEnabled());
}

}  // namespace snapper

QTEST_MAIN(snapper::SlotPickerTests)
#include "slot_picker_tests.moc"
