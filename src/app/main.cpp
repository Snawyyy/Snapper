#include <QApplication>

#include <cassert>

#include "app/app_context.h"

int main(int argc, char* argv[]) {
  assert(argc >= 1);
  assert(argv != nullptr);
  QApplication application(argc, argv);
  // Data paths become <data>/Snapper/Snapper; the Krita exporter writes
  // dolls there.
  QApplication::setOrganizationName(QStringLiteral("Snapper"));
  QApplication::setApplicationName(QStringLiteral("Snapper"));
  snapper::AppContext context;
  context.window()->show();
  return QApplication::exec();
}
