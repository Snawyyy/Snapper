#ifndef SNAPPER_BASE_TEXT_H_
#define SNAPPER_BASE_TEXT_H_

#include <QString>

namespace snapper {

// User-facing text from code that has no QObject to call tr() on. One
// translation context keeps every such message in one place.
QString Tr(const char* text);

}  // namespace snapper

#endif  // SNAPPER_BASE_TEXT_H_
