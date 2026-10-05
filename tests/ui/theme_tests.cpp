#include <QApplication>
#include <QPushButton>
#include <QStyle>
#include <QTest>

#include "ui/theme.h"

namespace snapper {

class ThemeTests final : public QObject {
  Q_OBJECT

 private slots:
  void IsDarkWindowsWithTetoRed();
};

void ThemeTests::IsDarkWindowsWithTetoRed() {
  theme::Apply(qApp);
  QCOMPARE(QApplication::style()->name(), QString("windows"));
  const QPalette palette = QApplication::palette();
  QCOMPARE(palette.color(QPalette::Highlight), theme::kPick);
  QVERIFY(palette.color(QPalette::Window).lightness() < 80);
  QVERIFY(palette.color(QPalette::Text).lightness() > 200);
  QCOMPARE(palette.color(QPalette::Disabled, QPalette::Text),
           theme::kTextOff);
  QCOMPARE(QApplication::font().pointSize(), theme::kFontPoints);
}

}  // namespace snapper

QTEST_MAIN(snapper::ThemeTests)
#include "theme_tests.moc"
