#include <QApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>

#include "edit/character_manager.h"
#include "edit/history_manager.h"
#include "ui/character_page.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Saves widget as name.png into $SNAPPER_SHOTS, when it is set, so the
// page can be looked at without starting the app.
void Shoot(QWidget* widget, const QString& name) {
  const QString folder = qEnvironmentVariable("SNAPPER_SHOTS");
  if (folder.isEmpty()) {
    return;
  }
  QDir().mkpath(folder);
  widget->grab().save(QDir(folder).filePath(name + ".png"));
}

void Drag(QWidget* widget, QPoint from, QPoint to,
          Qt::KeyboardModifiers mods = Qt::NoModifier) {
  QTest::mousePress(widget, Qt::LeftButton, mods, from);
  for (int step = 1; step <= 8; ++step) {
    const QPoint at = from + (to - from) * step / 8;
    QMouseEvent move(QEvent::MouseMove, at, widget->mapToGlobal(at),
                     Qt::NoButton, Qt::LeftButton, mods);
    QApplication::sendEvent(widget, &move);
  }
  QTest::mouseRelease(widget, Qt::LeftButton, mods, to);
}

}  // namespace

class CharacterPageTests final : public QObject {
  Q_OBJECT

 private slots:
  void DrawingAndRiggingByMouse();
  void ShapingToolsReshapeLines();
};

void CharacterPageTests::DrawingAndRiggingByMouse() {
  theme::Apply(qApp);
  QTemporaryDir library;
  HistoryManager history{Project()};
  CharacterManager characters(&history, library.path());
  CharacterPage page(&history, &characters, QString());
  page.resize(1400, 820);
  page.show();
  QVERIFY(QTest::qWaitForWindowExposed(&page));
  Shoot(&page, "01_empty");
  const auto made = characters.Create("Bo");
  QVERIFY(made.has_value());
  page.session()->SetCharacter(*made);
  const Character* bo = FindCharacter(history.current(), "Bo");
  const PartId drawing = bo->parts[1].id;
  const PartId bones = bo->parts[0].id;
  page.session()->SetPart(drawing);
  CharacterCanvas* canvas = page.canvas();
  const QPoint middle = canvas->rect().center();
  page.session()->SetTool(CharacterTool::kOval);
  Drag(canvas, middle + QPoint(-90, -160), middle + QPoint(90, 30));
  VectorShape style = page.session()->style();
  style.fill = QColor(240, 200, 170);
  page.session()->SetStyle(style);
  page.session()->SetTool(CharacterTool::kRectangle);
  Drag(canvas, middle + QPoint(-60, 40), middle + QPoint(60, 220));
  page.session()->SetTool(CharacterTool::kFreehand);
  Drag(canvas, middle + QPoint(-60, -60), middle + QPoint(60, -40));
  bo = FindCharacter(history.current(), "Bo");
  QCOMPARE(FindPart(*bo, drawing)->drawing.shapes.size(), size_t{3});
  Shoot(&page, "02_drawn");
  page.session()->SetPart(bones);
  page.session()->SetTool(CharacterTool::kAddPoint);
  Drag(canvas, middle + QPoint(0, 40), middle + QPoint(0, 130));
  Drag(canvas, middle + QPoint(0, 130), middle + QPoint(0, 220));
  bo = FindCharacter(history.current(), "Bo");
  QCOMPARE(FindPart(*bo, bones)->bones.size(), size_t{2});
  QCOMPARE(FindPart(*bo, bones)->bones[1].parent, FindPart(*bo, bones)->bones[0].id);
  Shoot(&page, "03_bones");
  page.session()->SetTool(CharacterTool::kTryBones);
  QTest::mousePress(canvas, Qt::LeftButton, {}, middle + QPoint(0, 220));
  for (int step = 1; step <= 8; ++step) {
    const QPoint at = middle + QPoint(step * 12, 220 - step * 4);
    QMouseEvent move(QEvent::MouseMove, at, canvas->mapToGlobal(at),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas, &move);
  }
  Shoot(&page, "04_trying");
  QTest::mouseRelease(canvas, Qt::LeftButton, {}, middle + QPoint(96, 188));
  QVERIFY(canvas->ShownPose().bones.empty());
  // A smart bone whose stop at 60 degrees moves the head up.
  page.session()->SetTool(CharacterTool::kAddPoint);
  Drag(canvas, middle + QPoint(160, -100), middle + QPoint(160, -160));
  bo = FindCharacter(history.current(), "Bo");
  const BoneId smart = FindPart(*bo, bones)->bones.back().id;
  QVERIFY(characters.SetSmart("Bo", smart, true).has_value());
  const auto stop = characters.AddActionStop("Bo", smart, 60.0);
  QVERIFY(stop.has_value());
  page.session()->SetAction(smart, *stop, 60.0);
  page.session()->SetPart(drawing);
  page.session()->SetTool(CharacterTool::kPick);
  const QPoint head = middle + QPoint(0, -65);
  Drag(canvas, head + QPoint(-120, -120), head + QPoint(120, 120));
  QVERIFY(!page.session()->points().empty());
  Drag(canvas, middle + QPoint(-90, -65), middle + QPoint(-90, -125));
  bo = FindCharacter(history.current(), "Bo");
  QVERIFY(!FindAction(*bo, smart)->stops[0].pose.parts.empty());
  Shoot(&page, "05_smart_stop");
  page.session()->SetAction(smart, -1, 30.0);
  Shoot(&page, "06_smart_half");
}

void CharacterPageTests::ShapingToolsReshapeLines() {
  QTemporaryDir library;
  HistoryManager history{Project()};
  CharacterManager characters(&history, library.path());
  CharacterPage page(&history, &characters, QString());
  page.resize(1400, 820);
  page.show();
  QVERIFY(QTest::qWaitForWindowExposed(&page));
  const auto made = characters.Create("Bo");
  QVERIFY(made.has_value());
  page.session()->SetCharacter(*made);
  const PartId drawing = FindCharacter(history.current(), "Bo")->parts[1].id;
  page.session()->SetPart(drawing);
  CharacterCanvas* canvas = page.canvas();
  const QPoint middle = canvas->rect().center();
  const QPoint corner = middle + QPoint(-60, -60);
  page.session()->SetTool(CharacterTool::kRectangle);
  Drag(canvas, corner, middle + QPoint(60, 60));
  const auto shape = [&history, drawing] {
    return FindPart(*FindCharacter(history.current(), "Bo"), drawing)->drawing;
  };
  QCOMPARE(shape().points.size(), size_t{4});
  // Line width: dragging up from a corner thickens the line there only.
  page.session()->SetPoints({});
  page.session()->SetTool(CharacterTool::kLineWidth);
  Drag(canvas, corner, corner + QPoint(0, -120));
  QCOMPARE(history.UndoLabel(), QString("Line width"));
  int thick = 0;
  for (const VectorPoint& point : shape().points) {
    thick += point.width > 1.5 ? 1 : 0;
  }
  QCOMPARE(thick, 1);
  // Delete edge: a click on the top side opens the square, its fill goes.
  page.session()->SetTool(CharacterTool::kDeleteEdge);
  QTest::mouseClick(canvas, Qt::LeftButton, {}, middle + QPoint(0, -60));
  QCOMPARE(history.UndoLabel(), QString("Delete edge"));
  QVERIFY(shape().shapes.empty());
  QVERIFY(!shape().curves[0].is_closed);
  // Magnet: a drag from the bottom right corner pulls it all the way and
  // its neighbours less, the far corner not at all.
  page.session()->SetTool(CharacterTool::kMagnet);
  const auto place_of = [&shape](PointId id) {
    for (const VectorPoint& point : shape().points) {
      if (point.id == id) return point.at;
    }
    return QPointF();
  };
  const VectorDrawing before = shape();
  Drag(canvas, middle + QPoint(60, 60), middle + QPoint(90, 60));
  QCOMPARE(history.UndoLabel(), QString("Magnet"));
  double most = 0.0;
  int held = 0;
  for (const VectorPoint& point : before.points) {
    const QPointF now = place_of(point.id);
    most = std::max(most, now.x() - point.at.x());
    held += now == point.at ? 1 : 0;
  }
  QVERIFY(most > 1.0);
  QVERIFY(held >= 1);
}

}  // namespace snapper

QTEST_MAIN(snapper::CharacterPageTests)
#include "character_page_tests.moc"
