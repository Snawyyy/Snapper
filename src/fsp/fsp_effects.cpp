#include "fsp/fsp_effects.h"

#include <QColor>

#include <cassert>
#include <cstddef>
#include <vector>

#include "fsp_host.h"

namespace snapper {
namespace {

// Bounds on what the plugin list can hold, so every loop over it ends.
constexpr int kMaxPlugins = 1024;
constexpr int kMaxParams = 512;
constexpr int kMaxChoices = 1024;
// Bytes a pixel in After Effects' 8 bit layout: alpha, red, green, blue.
constexpr int kArgbBytes = 4;

FspParamType TypeOf(int host_type) {
  assert(host_type >= FSP_HOST_PARAM_SLIDER);
  assert(host_type <= FSP_HOST_PARAM_OTHER);
  switch (host_type) {
    case FSP_HOST_PARAM_FLOAT:
      return FspParamType::kFloat;
    case FSP_HOST_PARAM_ANGLE:
      return FspParamType::kAngle;
    case FSP_HOST_PARAM_CHECKBOX:
      return FspParamType::kCheckbox;
    case FSP_HOST_PARAM_POPUP:
      return FspParamType::kPopup;
    case FSP_HOST_PARAM_COLOR:
      return FspParamType::kColor;
    case FSP_HOST_PARAM_POINT:
      return FspParamType::kPoint;
    default:
      return FspParamType::kSlider;
  }
}

FspParam ReadParam(const FspHostParam& host) {
  assert(host.name != nullptr);
  assert(host.slot >= 0);
  FspParam param;
  param.name = QString::fromUtf8(host.name);
  param.type = TypeOf(host.type);
  param.low = host.low;
  param.high = host.high;
  param.slider_low = host.slider_low;
  param.slider_high = host.slider_high;
  param.neutral = host.neutral;
  param.neutral_y = host.neutral_y;
  param.slot = host.slot;
  param.is_hidden = host.is_hidden != 0;
  const int choice_count = host.choice_count < kMaxChoices
                               ? host.choice_count
                               : kMaxChoices;
  for (int i = 0; i < choice_count; ++i) {
    param.choices.append(QString::fromUtf8(host.choices[i]));
  }
  return param;
}

FspEffectInfo ReadPlugin(int plugin) {
  assert(plugin >= 0);
  assert(plugin < fsp_host_plugin_count());
  FspEffectInfo info;
  info.id = QString::fromUtf8(fsp_host_plugin_id(plugin));
  info.name = QString::fromUtf8(fsp_host_plugin_name(plugin));
  info.slot_count = fsp_host_slot_count(plugin);
  const int count = fsp_host_param_count(plugin);
  const int bounded = count < kMaxParams ? count : kMaxParams;
  for (int i = 0; i < bounded; ++i) {
    const FspHostParam* host = fsp_host_param(plugin, i);
    const bool has_param = host != nullptr;
    if (has_param) {
      info.params.push_back(ReadParam(*host));
    }
  }
  return info;
}

std::vector<FspEffectInfo> ReadAll() {
  const int count = fsp_host_plugin_count();
  assert(count >= 0);
  const int bounded = count < kMaxPlugins ? count : kMaxPlugins;
  std::vector<FspEffectInfo> all;
  all.reserve(static_cast<std::size_t>(bounded));
  for (int i = 0; i < bounded; ++i) {
    all.push_back(ReadPlugin(i));
  }
  assert(static_cast<int>(all.size()) == bounded);
  return all;
}

// The host's number for the plugin named id, -1 when it has none.
int HostIndex(const QString& id) {
  assert(!id.isEmpty());
  const int count = fsp_host_plugin_count();
  assert(count >= 0);
  const int bounded = count < kMaxPlugins ? count : kMaxPlugins;
  for (int i = 0; i < bounded; ++i) {
    const bool is_it = id == QString::fromUtf8(fsp_host_plugin_id(i));
    if (is_it) {
      return i;
    }
  }
  return -1;
}

std::size_t ByteCount(const QImage& image) {
  assert(image.width() > 0);
  assert(image.height() > 0);
  return static_cast<std::size_t>(image.width()) *
         static_cast<std::size_t>(image.height()) * kArgbBytes;
}

// Straight alpha bytes, as After Effects lays out an 8 bit layer.
std::vector<unsigned char> ToArgb(const QImage& straight) {
  assert(straight.format() == QImage::Format_ARGB32);
  std::vector<unsigned char> bytes(ByteCount(straight));
  const int width = straight.width();
  for (int y = 0; y < straight.height(); ++y) {
    const QRgb* row =
        reinterpret_cast<const QRgb*>(straight.constScanLine(y));
    std::size_t at = static_cast<std::size_t>(y) *
                     static_cast<std::size_t>(width) * kArgbBytes;
    for (int x = 0; x < width; ++x) {
      bytes[at++] = static_cast<unsigned char>(qAlpha(row[x]));
      bytes[at++] = static_cast<unsigned char>(qRed(row[x]));
      bytes[at++] = static_cast<unsigned char>(qGreen(row[x]));
      bytes[at++] = static_cast<unsigned char>(qBlue(row[x]));
    }
  }
  assert(!bytes.empty());
  return bytes;
}

QImage FromArgb(const std::vector<unsigned char>& bytes, int width,
                int height) {
  assert(width > 0 && height > 0);
  QImage image(width, height, QImage::Format_ARGB32);
  assert(bytes.size() == ByteCount(image));
  for (int y = 0; y < height; ++y) {
    QRgb* row = reinterpret_cast<QRgb*>(image.scanLine(y));
    std::size_t at = static_cast<std::size_t>(y) *
                     static_cast<std::size_t>(width) * kArgbBytes;
    for (int x = 0; x < width; ++x) {
      row[x] = qRgba(bytes[at + 1], bytes[at + 2], bytes[at + 3],
                     bytes[at]);
      at += kArgbBytes;
    }
  }
  return image;
}

}  // namespace

const std::vector<FspEffectInfo>& AllFspEffects() {
  static const std::vector<FspEffectInfo> all = ReadAll();
  assert(all.size() <= static_cast<std::size_t>(kMaxPlugins));
  assert(static_cast<int>(all.size()) <= fsp_host_plugin_count());
  return all;
}

const FspEffectInfo* FindFspEffect(const QString& id) {
  assert(!id.isEmpty());
  const std::vector<FspEffectInfo>& all = AllFspEffects();
  assert(all.size() <= static_cast<std::size_t>(kMaxPlugins));
  for (const FspEffectInfo& info : all) {
    const bool is_it = info.id == id;
    if (is_it) {
      return &info;
    }
  }
  return nullptr;
}

FspValues DefaultFspValues(const FspEffectInfo& info) {
  assert(info.slot_count >= 0);
  assert(info.params.size() <= static_cast<std::size_t>(kMaxParams));
  FspValues values(static_cast<std::size_t>(info.slot_count), 0.0);
  for (const FspParam& param : info.params) {
    const std::size_t slot = static_cast<std::size_t>(param.slot);
    const bool is_point = param.type == FspParamType::kPoint;
    const std::size_t needed = slot + (is_point ? 2 : 1);
    const bool fits = needed <= values.size();
    if (!fits) {
      continue;
    }
    values[slot] = param.neutral;
    if (is_point) {
      values[slot + 1] = param.neutral_y;
    }
  }
  return values;
}

QImage RunFspEffect(const FspEffectInfo& info, const FspValues& values,
                    const QImage& input, double scale) {
  assert(scale > 0.0);
  assert(!info.id.isEmpty());
  const bool fits = static_cast<int>(values.size()) == info.slot_count &&
                    !input.isNull() && input.width() > 0 &&
                    input.height() > 0;
  if (!fits) {
    return QImage();
  }
  const int plugin = HostIndex(info.id);
  const bool is_known = plugin >= 0;
  if (!is_known) {
    return QImage();
  }
  const QImage straight = input.convertToFormat(QImage::Format_ARGB32);
  const std::vector<unsigned char> in = ToArgb(straight);
  std::vector<unsigned char> out(in.size());
  const int err = fsp_host_render(
      plugin, straight.width(), straight.height(), in.data(), out.data(),
      values.data(), static_cast<int>(values.size()), scale);
  const bool has_failed = err != 0;
  if (has_failed) {
    return QImage();
  }
  return FromArgb(out, straight.width(), straight.height());
}

}  // namespace snapper
