#ifndef SNAPPER_IO_JSON_VALUES_H_
#define SNAPPER_IO_JSON_VALUES_H_

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QSize>
#include <QString>

#include <cassert>
#include <span>

#include "model/key.h"
#include "model/pose.h"

namespace snapper {

// Collects the first problem met while reading a file, so readers can
// keep going with defaults and report once at the end.
class JsonIssues final {
 public:
  void Note(const QString& problem);
  bool HasIssue() const { return !first_.isEmpty(); }
  const QString& first() const { return first_; }

 private:
  QString first_;
};

QJsonArray PointToJson(QPointF point);
QPointF PointFromJson(const QJsonValue& value);
QJsonArray SizeToJson(QSize size);
QSize SizeFromJson(const QJsonValue& value);
QString ColorToJson(const QColor& color);
QColor ColorFromJson(const QJsonValue& value, QColor fallback,
                     JsonIssues* issues);

// Enums are stored by name, so files survive reordering the enum.
// names[i] is the name of value i.
QString EnumToJson(int value, std::span<const char* const> names);
int EnumFromJson(const QJsonValue& value, std::span<const char* const> names,
                 JsonIssues* issues);

QJsonValue ValueToJson(double value);
double ValueFromJson(const QJsonValue& value, double fallback,
                     JsonIssues* issues);
QJsonValue ValueToJson(const PiecePose& pose);
PiecePose ValueFromJson(const QJsonValue& value, PiecePose fallback,
                        JsonIssues* issues);
QJsonValue ValueToJson(const CameraPose& pose);
CameraPose ValueFromJson(const QJsonValue& value, CameraPose fallback,
                         JsonIssues* issues);

QString EaseToJson(Ease ease);
Ease EaseFromJson(const QJsonValue& value, JsonIssues* issues);

template <typename T>
QJsonArray ChannelToJson(const Channel<T>& channel) {
  assert(IsSorted(channel));
  assert(channel.keys.size() <= static_cast<size_t>(kMaxKeysPerChannel));
  QJsonArray keys;
  for (const Key<T>& key : channel.keys) {
    keys.append(QJsonObject{{"f", key.frame.index()},
                            {"e", EaseToJson(key.ease)},
                            {"v", ValueToJson(key.value)}});
  }
  return keys;
}

// Keys come back sorted and unique whatever order the file has.
template <typename T>
Channel<T> ChannelFromJson(const QJsonValue& value, const T& fallback,
                           JsonIssues* issues) {
  assert(issues != nullptr);
  const QJsonArray keys = value.toArray();
  assert(keys.size() >= 0);
  Channel<T> channel;
  const bool is_too_long = keys.size() > kMaxKeysPerChannel;
  if (is_too_long) {
    issues->Note(QStringLiteral("a channel has too many keys"));
    return channel;
  }
  for (const QJsonValue& item : keys) {
    const QJsonObject key = item.toObject();
    SetKey(&channel, Key<T>{Frame(key.value("f").toInt()),
                            ValueFromJson(key.value("v"), fallback, issues),
                            EaseFromJson(key.value("e"), issues)});
  }
  return channel;
}

}  // namespace snapper

#endif  // SNAPPER_IO_JSON_VALUES_H_
