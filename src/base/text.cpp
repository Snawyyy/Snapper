#include "base/text.h"

#include <QCoreApplication>

#include <cassert>

namespace snapper {

QString Tr(const char* text) {
  assert(text != nullptr);
  assert(text[0] != '\0');
  return QCoreApplication::translate("Snapper", text);
}

}  // namespace snapper
