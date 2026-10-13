#include "io/reference_json.h"

#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>

namespace snapper {
namespace {

// ReferenceTint by its value.
constexpr std::array<const char*, kReferenceTintCount> kTintNames = {
    "none", "blue", "red"};

QJsonObject ReferenceToJson(const Reference& reference) {
  assert(reference.id.IsValid());
  assert(reference.speed > 0.0);
  QJsonObject object{{"id", reference.id.value()},
                     {"path", reference.path},
                     {"size", SizeToJson(reference.size)},
                     {"seconds", reference.seconds},
                     {"opacity", reference.opacity},
                     {"in_front", reference.is_in_front},
                     {"shown", reference.is_shown},
                     {"flipped", reference.is_flipped},
                     {"tint", EnumToJson(static_cast<int>(reference.tint),
                                         kTintNames)},
                     {"center", PointToJson(reference.center)},
                     {"scale", reference.scale},
                     {"start", reference.start.index()},
                     {"skip", reference.skip},
                     {"speed", reference.speed}};
  const bool has_marks = !reference.marks.empty();
  if (has_marks) {
    QJsonArray marks;
    for (const double mark : reference.marks) {
      marks.append(mark);
    }
    object.insert("marks", marks);
  }
  return object;
}

Reference ReferenceFromJson(const QJsonObject& object, JsonIssues* issues) {
  assert(issues != nullptr);
  Reference reference;
  reference.id = ReferenceId(std::max(object.value("id").toInt(), 0));
  reference.path = object.value("path").toString();
  reference.size = SizeFromJson(object.value("size"));
  reference.seconds = std::clamp(object.value("seconds").toDouble(0.0), 0.0,
                                 static_cast<double>(kMaxFrame));
  reference.opacity = std::clamp(
      object.value("opacity").toDouble(kDefaultReferenceOpacity), 0.0, 1.0);
  reference.is_in_front = object.value("in_front").toBool(false);
  reference.is_shown = object.value("shown").toBool(true);
  reference.is_flipped = object.value("flipped").toBool(false);
  const bool has_tint = object.contains("tint");
  if (has_tint) {
    reference.tint = static_cast<ReferenceTint>(
        EnumFromJson(object.value("tint"), kTintNames, issues));
  }
  reference.center = PointFromJson(object.value("center"));
  reference.scale = std::clamp(object.value("scale").toDouble(1.0),
                               kMinReferenceScale, kMaxReferenceScale);
  reference.start = Frame(object.value("start").toInt());
  reference.skip = std::clamp(object.value("skip").toDouble(0.0), 0.0,
                              reference.seconds);
  reference.speed = std::clamp(object.value("speed").toDouble(1.0),
                               kMinReferenceSpeed, kMaxReferenceSpeed);
  const QJsonArray marks =
      Bounded(object.value("marks"), kMaxReferenceMarks, "marks", issues);
  for (const QJsonValue& mark : marks) {
    reference.marks.push_back(std::max(mark.toDouble(), 0.0));
  }
  std::ranges::sort(reference.marks);
  assert(reference.speed > 0.0);
  return reference;
}

}  // namespace

QJsonArray ReferencesToJson(const std::vector<Reference>& references) {
  assert(references.size() <= static_cast<size_t>(kMaxReferencesPerShot));
  QJsonArray array;
  for (const Reference& reference : references) {
    array.append(ReferenceToJson(reference));
  }
  assert(array.size() == static_cast<qsizetype>(references.size()));
  return array;
}

std::vector<Reference> ReferencesFromJson(const QJsonValue& value,
                                          JsonIssues* issues) {
  assert(issues != nullptr);
  const QJsonArray array =
      Bounded(value, kMaxReferencesPerShot, "references", issues);
  std::vector<Reference> references;
  for (const QJsonValue& item : array) {
    Reference reference = ReferenceFromJson(item.toObject(), issues);
    // A missing or repeated id gets a fresh one, so each is told apart.
    const bool needs_id = !reference.id.IsValid() ||
                          FindReference(references, reference.id) != nullptr;
    if (needs_id) {
      reference.id = NextReferenceId(references);
    }
    references.push_back(std::move(reference));
  }
  assert(references.size() <= static_cast<size_t>(kMaxReferencesPerShot));
  return references;
}

}  // namespace snapper
