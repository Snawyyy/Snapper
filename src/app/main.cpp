#include <QApplication>
#include <QStandardPaths>

#include <cassert>

#include "app/app_context.h"
#include "ui/theme.h"

int main(int argc, char* argv[]) {
  assert(argc >= 1);
  assert(argv != nullptr);
  QApplication application(argc, argv);
  // Data paths become <data>/Snapper/Snapper; the Krita exporter writes
  // dolls into its dolls folder.
  QApplication::setOrganizationName(QStringLiteral("Snapper"));
  QApplication::setApplicationName(QStringLiteral("Snapper"));
  snapper::theme::Apply(&application);
  const QString data =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  snapper::AppContext context(data);
  context.window()->show();
  return QApplication::exec();
}
