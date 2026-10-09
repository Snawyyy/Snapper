#include "ui/bar_text.h"

#include <cassert>

#include "base/text.h"

namespace snapper {

StepFace MakeStepFace(const QString& verb, const QString& label,
                      const QString& why_not) {
  assert(!verb.isEmpty());
  assert(label.size() >= 0);
  const bool can_act = why_not.isEmpty();
  StepFace face;
  face.enabled = can_act;
  face.text = can_act ? verb + QLatin1Char(' ') + label : verb;
  face.tooltip = why_not;
  return face;
}

QString FrameCounterText(Frame frame) {
  assert(frame.index() >= 0);
  assert(frame.index() <= kMaxFrame);
  return QStringLiteral("F %1").arg(frame.index(), 3, 10, QLatin1Char('0'));
}

QString TimecodeText(Frame frame) {
  assert(frame.index() >= 0);
  assert(frame.index() <= kMaxFrame);
  const int index = frame.index();
  const int seconds = index / kFramesPerSecond;
  return QStringLiteral("%1:%2.%3")
      .arg(seconds / 60)
      .arg(seconds % 60, 2, 10, QLatin1Char('0'))
      .arg(index % kFramesPerSecond, 2, 10, QLatin1Char('0'));
}

QString PlayText(bool is_playing) {
  assert(!Tr("Play").isEmpty());
  assert(!Tr("Pause").isEmpty());
  return is_playing ? Tr("Pause") : Tr("Play");
}

}  // namespace snapper
