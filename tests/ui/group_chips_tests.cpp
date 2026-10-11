#include <QTest>

#include "ui/group_chips.h"

namespace snapper {
namespace {

WarpDot Dot(int point) {
  assert(point >= 0);
  assert(point < 100);
  return WarpDot{LayerId(1), "head", point};
}

}  // namespace

class GroupChipsTests final : public QObject {
  Q_OBJECT

 private slots:
  void SaysHowMuchOfTheGroupIsPicked();
  void ADefaultSaysSoInItsTip();
  void KeysChooseHowAClickPicks();
};

void GroupChipsTests::SaysHowMuchOfTheGroupIsPicked() {
  const ShownGroup group{"Still", {Dot(1), Dot(2), Dot(3)}, false};
  const ChipLook none = ChipOf(group, {Dot(7)});
  QCOMPARE(none.share, GroupShare::kNone);
  QCOMPARE(none.text, QString("Still (3)"));
  QCOMPARE(ChipOf(group, {Dot(1), Dot(7)}).share, GroupShare::kSome);
  const ChipLook all = ChipOf(group, {Dot(3), Dot(2), Dot(1), Dot(9)});
  QCOMPARE(all.share, GroupShare::kAll);
  QVERIFY(all.tip.contains("3 picked"));
  // A group with no dots is never wholly picked.
  QCOMPARE(ChipOf({"Empty", {}, false}, {Dot(1)}).share, GroupShare::kNone);
}

void GroupChipsTests::ADefaultSaysSoInItsTip() {
  QVERIFY(ChipOf({"A", {Dot(1)}, true}, {}).tip.contains("default"));
  QVERIFY(!ChipOf({"A", {Dot(1)}, false}, {}).tip.contains("default"));
}

void GroupChipsTests::KeysChooseHowAClickPicks() {
  QCOMPARE(GroupPickMode(false, false), PickMode::kReplace);
  QCOMPARE(GroupPickMode(true, false), PickMode::kAdd);
  QCOMPARE(GroupPickMode(false, true), PickMode::kToggle);
  QCOMPARE(GroupPickMode(true, true), PickMode::kToggle);
}

}  // namespace snapper

QTEST_MAIN(snapper::GroupChipsTests)
#include "group_chips_tests.moc"
