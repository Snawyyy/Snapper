#ifndef SNAPPER_UI_REFERENCE_KEYS_H_
#define SNAPPER_UI_REFERENCE_KEYS_H_

#include <QKeyEvent>
#include <QString>

#include "ui/managers.h"

namespace snapper {

// The Pose tab's reference keys, shared by the stage and the timeline:
// Y shows or hides every reference; M with a reference picked marks the
// moment of it under the playhead. True when the key was one of them and
// is dealt with; problem gets why it did nothing, or empty.
bool ReferenceKey(const QKeyEvent& event, const Managers& managers,
                  QString* problem);

}  // namespace snapper

#endif  // SNAPPER_UI_REFERENCE_KEYS_H_
