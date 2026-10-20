#include <QImage>
#include <QPainter>
#include <QTest>

#include <cassert>

#include "anim/character_pose.h"
#include "render/character_paint.h"

namespace snapper {
namespace {

// A character whose only drawing is a 10 pixel line from (10, 20) to
// (190, 20) with brush.
Character Line(LineBrush brush) {
  Character character;
  character.name = "Line";
  CharacterPart part;
  part.id = PartId(1);
  part.name = "Drawing";
  part.kind = PartKind::kVector;
  part.drawing.points = {VectorPoint{PointId(2), {10, 20}, 0.0},
                         VectorPoint{PointId(3), {190, 20}, 0.0}};
  part.drawing.curves = {{{PointId(2), PointId(3)}, false}};
  part.drawing.shapes = {VectorShape{.curves = {0},
                                     .has_fill = false,
                                     .stroke = Qt::black,
                                     .stroke_width = 10.0,
                                     .brush = brush}};
  character.parts = {part};
  character.next_id = 4;
  assert(IsSound(character));
  return character;
}

QImage Drawn(LineBrush brush) {
  const Character character = Line(brush);
  QImage image(200, 40, QImage::Format_ARGB32_Premultiplied);
  image.fill(Qt::transparent);
  QPainter painter(&image);
  ImageCache cache;
  PaintCharacter(character, PoseCharacter(character, CharacterPose{}),
                 QTransform(), &cache, &painter);
  painter.end();
  return image;
}

// The darkest and lightest alpha along the middle of the line.
std::pair<int, int> AlphaRange(const QImage& image) {
  int most = 0;
  int least = 255;
  for (int x = 30; x < 170; ++x) {
    const int alpha = qAlpha(image.pixel(x, 20));
    most = std::max(most, alpha);
    least = std::min(least, alpha);
  }
  return {most, least};
}

}  // namespace

class LineBrushTests final : public QObject {
  Q_OBJECT

 private slots:
  void EachBrushLaysItsOwnLine();
};

void LineBrushTests::EachBrushLaysItsOwnLine() {
  const auto solid = AlphaRange(Drawn(LineBrush::kSolid));
  QCOMPARE(solid.first, 255);
  QCOMPARE(solid.second, 255);
  // Dots leave clear gaps between them.
  const auto dots = AlphaRange(Drawn(LineBrush::kDots));
  QCOMPARE(dots.first, 255);
  QCOMPARE(dots.second, 0);
  // Marker is see-through everywhere.
  const auto marker = AlphaRange(Drawn(LineBrush::kMarker));
  QVERIFY(marker.first > 40 && marker.first < 230);
  // Ink is thin at its ends and full in the middle.
  const QImage ink = Drawn(LineBrush::kInk);
  QCOMPARE(qAlpha(ink.pixel(100, 24)), 255);
  QVERIFY(qAlpha(ink.pixel(12, 24)) < 128);
  // Pencil and chalk are grainy: never one even tone, and the same every
  // time they are drawn.
  for (const LineBrush brush : {LineBrush::kPencil, LineBrush::kChalk}) {
    const QImage once = Drawn(brush);
    const auto range = AlphaRange(once);
    QVERIFY(range.first > 0);
    QVERIFY(range.first != range.second);
    QCOMPARE(Drawn(brush), once);
  }
}

}  // namespace snapper

QTEST_MAIN(snapper::LineBrushTests)
#include "line_brush_tests.moc"
