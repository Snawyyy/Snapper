#include <QTest>

#include <limits>
#include <type_traits>
#include <unordered_set>

#include "base/frame.h"
#include "base/id.h"

namespace snapper {
namespace {

struct PieceTag;
struct ShotTag;
using PieceId = Id<PieceTag>;

}  // namespace

class BaseTests final : public QObject {
  Q_OBJECT

 private slots:
  void IdsStartInvalidAndCompare();
  void FramesClampToTheirRange();
  void SecondsRoundDownToTheFrameThatShowsThem();
  void BadSecondsMeanTheStart();
};

void BaseTests::IdsStartInvalidAndCompare() {
  static_assert(!std::is_convertible_v<Id<ShotTag>, PieceId>);
  QVERIFY(!PieceId().IsValid());
  QVERIFY(PieceId(1).IsValid());
  QVERIFY(PieceId(1) < PieceId(2));
  const std::unordered_set<PieceId> ids = {PieceId(3), PieceId(3)};
  QCOMPARE(ids.size(), size_t{1});
}

void BaseTests::FramesClampToTheirRange() {
  QCOMPARE(Frame(-5).index(), 0);
  QCOMPARE(Frame(kMaxFrame + 1).index(), kMaxFrame);
  QVERIFY(Frame(2) < Frame(3));
  QCOMPARE(SecondsAtFrame(Frame(48)), 2.0);
}

void BaseTests::SecondsRoundDownToTheFrameThatShowsThem() {
  QCOMPARE(FrameAtSeconds(1.0).index(), 24);
  // 1.04 s is inside frame 24 (1.0 s to 1.0417 s).
  QCOMPARE(FrameAtSeconds(1.04).index(), 24);
  QCOMPARE(FrameAtSeconds(1e12).index(), kMaxFrame);
}

void BaseTests::BadSecondsMeanTheStart() {
  QCOMPARE(FrameAtSeconds(-1.0).index(), 0);
  QCOMPARE(FrameAtSeconds(std::numeric_limits<double>::quiet_NaN()).index(),
           0);
  QCOMPARE(FrameAtSeconds(std::numeric_limits<double>::infinity()).index(),
           0);
}

}  // namespace snapper

QTEST_APPLESS_MAIN(snapper::BaseTests)
#include "base_tests.moc"
