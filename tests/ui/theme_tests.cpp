#include <QApplication>
#include <QProxyStyle>
#include <QPushButton>
#include <QStyle>
#include <QTest>

#include "ui/snapper_style.h"
#include "ui/theme.h"

namespace snapper {

class ThemeTests final : public QObject {
  Q_OBJECT

 private slots:
  void IsDarkWindowsWithTetoRed();
  void PressSinksAndHoverBrightens();
  void ADeadButtonNeverReacts();
  void PressSquashesFourPercent();
  void ButtonPaintsItsHover();
};

void ThemeTests::IsDarkWindowsWithTetoRed() {
  theme::Apply(qApp);
  const auto* style = qobject_cast<QProxyStyle*>(QApplication::style());
  QVERIFY(style != nullptr);
  QCOMPARE(style->baseStyle()->name(), QString("windows"));
  const QPalette palette = QApplication::palette();
  QCOMPARE(palette.color(QPalette::Highlight), theme::kPick);
  QVERIFY(palette.color(QPalette::Window).lightness() < 80);
  QVERIFY(palette.color(QPalette::Text).lightness() > 200);
  QCOMPARE(palette.color(QPalette::Disabled, QPalette::Text),
           theme::kTextOff);
  QCOMPARE(QApplication::font().pointSize(), theme::kFontPoints);
}

void ThemeTests::PressSinksAndHoverBrightens() {
  const ButtonLook rest = LookOfButton(true, false, false, 0.0);
  const ButtonLook over = LookOfButton(true, false, false, 1.0);
  const ButtonLook down = LookOfButton(true, true, false, 1.0);
  const ButtonLook latched = LookOfButton(true, false, true, 0.0);
  QVERIFY(!rest.is_sunk);
  QVERIFY(over.fill.lightness() > rest.fill.lightness());
  QVERIFY(down.is_sunk);
  QVERIFY(down.fill.lightness() < rest.fill.lightness());
  QVERIFY(latched.is_sunk);
}

void ThemeTests::ADeadButtonNeverReacts() {
  const ButtonLook rest = LookOfButton(false, false, false, 0.0);
  const ButtonLook poked = LookOfButton(false, true, false, 1.0);
  QCOMPARE(poked.fill, rest.fill);
  QVERIFY(!poked.is_sunk);
}

void ThemeTests::PressSquashesFourPercent() {
  QCOMPARE(SquashScale(0.0), 1.0);
  QCOMPARE(SquashScale(1.0), 0.96);
  // Mid spring the button is a touch bigger than at rest.
  QVERIFY(SquashScale(-0.2) > 1.0);
  QVERIFY(SquashScale(-50.0) <= 1.02);
}

void ThemeTests::ButtonPaintsItsHover() {
  theme::Apply(qApp);
  QPushButton button("Go");
  button.resize(80, 30);
  const QPoint middle(6, 15);
  const QColor rest = button.grab().toImage().pixelColor(middle);
  button.setProperty(kHoverProperty, 1.0);
  const QColor over = button.grab().toImage().pixelColor(middle);
  QVERIFY(over.lightness() > rest.lightness());
  button.setDown(true);
  const QColor down = button.grab().toImage().pixelColor(middle);
  QVERIFY(down.lightness() < rest.lightness());
}

}  // namespace snapper

QTEST_MAIN(snapper::ThemeTests)
#include "theme_tests.moc"
