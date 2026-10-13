#include "model/reference.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// The file's moment at local, not held at its end.
double RawSeconds(const Reference& reference, Frame local) {
  assert(reference.speed > 0.0);
  assert(local.index() >= reference.start.index());
  const double played =
      static_cast<double>(local.index() - reference.start.index()) /
      kFramesPerSecond;
  return reference.skip + played * reference.speed;
}

}  // namespace

const Reference* FindReference(const std::vector<Reference>& references,
                               ReferenceId id) {
  assert(references.size() <= static_cast<size_t>(kMaxReferencesPerShot));
  assert(id.value() >= 0);
  const auto found = std::ranges::find_if(
      references, [id](const Reference& one) { return one.id == id; });
  return found == references.end() ? nullptr : &*found;
}

Reference* FindReference(std::vector<Reference>* references, ReferenceId id) {
  assert(references != nullptr);
  assert(id.value() >= 0);
  const Reference* found =
      FindReference(static_cast<const std::vector<Reference>&>(*references),
                    id);
  return const_cast<Reference*>(found);
}

ReferenceId NextReferenceId(const std::vector<Reference>& references) {
  assert(references.size() <= static_cast<size_t>(kMaxReferencesPerShot));
  int highest = 0;
  for (const Reference& one : references) {
    highest = std::max(highest, one.id.value());
  }
  assert(highest >= 0);
  return ReferenceId(highest + 1);
}

std::optional<double> ReferenceSecondsAt(const Reference& reference,
                                         Frame local) {
  assert(reference.speed > 0.0);
  assert(local.index() >= 0);
  const bool is_before = local < reference.start;
  if (is_before) {
    return std::nullopt;
  }
  if (reference.IsStill()) {
    return 0.0;
  }
  return std::clamp(RawSeconds(reference, local), 0.0, reference.seconds);
}

bool IsReferenceEnded(const Reference& reference, Frame local) {
  assert(reference.speed > 0.0);
  assert(local.index() >= 0);
  const bool is_playing = !reference.IsStill() && !(local < reference.start);
  return is_playing && RawSeconds(reference, local) >= reference.seconds;
}

double ReferenceFrameOf(const Reference& reference, double seconds) {
  assert(reference.speed > 0.0);
  assert(std::isfinite(seconds));
  return reference.start.index() +
         (seconds - reference.skip) / reference.speed * kFramesPerSecond;
}

double ReferenceEndFrame(const Reference& reference) {
  assert(reference.speed > 0.0);
  assert(reference.seconds >= 0.0);
  return reference.IsStill() ? reference.start.index()
                             : ReferenceFrameOf(reference, reference.seconds);
}

QRectF ReferenceRect(const Reference& reference, QSize canvas) {
  assert(reference.scale > 0.0);
  assert(canvas.width() > 0 && canvas.height() > 0);
  const QSizeF shown(reference.size.width() * reference.scale,
                     reference.size.height() * reference.scale);
  const QPointF middle(canvas.width() / 2.0, canvas.height() / 2.0);
  return QRectF(middle + reference.center -
                    QPointF(shown.width() / 2.0, shown.height() / 2.0),
                shown);
}

double FitFrameScale(const Reference& reference, QSize canvas) {
  assert(canvas.width() > 0 && canvas.height() > 0);
  assert(reference.size.width() >= 0 && reference.size.height() >= 0);
  const bool is_sized = !reference.size.isEmpty();
  if (!is_sized) {
    return 1.0;
  }
  return std::min(
      static_cast<double>(canvas.width()) / reference.size.width(),
      static_cast<double>(canvas.height()) / reference.size.height());
}

double FitHeightScale(const Reference& reference, QSize canvas) {
  assert(canvas.width() > 0 && canvas.height() > 0);
  assert(reference.size.width() >= 0 && reference.size.height() >= 0);
  const bool is_sized = reference.size.height() > 0;
  return is_sized ? static_cast<double>(canvas.height()) /
                        reference.size.height()
                  : 1.0;
}

}  // namespace snapper
