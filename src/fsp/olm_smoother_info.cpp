#include "fsp/olm_smoother_info.h"

#include <cassert>

namespace snapper {
namespace {

constexpr double kWhite = 0xFFFFFF;

FspParam Number(const char* name, FspParamType type, double high,
                double neutral) {
  assert(name != nullptr);
  assert(neutral >= 0.0 && neutral <= high);
  FspParam param;
  param.name = QString::fromLatin1(name);
  param.type = type;
  param.high = high;
  param.slider_high = high;
  param.neutral = neutral;
  return param;
}

FspParam Choice(const char* name, const QStringList& choices,
                double neutral) {
  assert(name != nullptr);
  assert(neutral >= 1.0 && neutral <= static_cast<double>(choices.size()));
  FspParam param;
  param.name = QString::fromLatin1(name);
  param.type = FspParamType::kPopup;
  param.low = 1.0;
  param.high = static_cast<double>(choices.size());
  param.neutral = neutral;
  param.choices = choices;
  return param;
}

FspParam Color(const QString& name, double neutral) {
  assert(!name.isEmpty());
  assert(neutral >= 0.0 && neutral <= kWhite);
  FspParam param;
  param.name = name;
  param.type = FspParamType::kColor;
  param.high = kWhite;
  param.neutral = neutral;
  return param;
}

FspEffectInfo Build() {
  FspEffectInfo info;
  info.id = QStringLiteral("OLMSmoother");
  info.name = QStringLiteral("OLM Smoother");
  std::vector<FspParam>& params = info.params;
  params.push_back(
      Number("Smoothness", FspParamType::kSlider, 100.0, 100.0));
  params.push_back(Number("Extra Smooth", FspParamType::kSlider, 100.0, 0.0));
  params.push_back(Number("Smooth Range", FspParamType::kSlider, 255.0, 0.0));
  params.push_back(Number("Use Color Key", FspParamType::kCheckbox, 1.0, 0.0));
  params.push_back(Color(QStringLiteral("Color Key"), kWhite));
  params.push_back(Choice("Smoother Version", {"v1", "v2"}, 2.0));
  params.push_back(Choice("Gamma Correction",
                          {"Off", "All Colors", "Gamma Colors"}, 1.0));
  FspParam gamma = Number("Gamma Value", FspParamType::kFloat, 10.0, 2.4);
  gamma.low = 0.1;
  gamma.slider_low = 0.5;
  gamma.slider_high = 4.0;
  params.push_back(gamma);
  params.push_back(Choice("Number of Gamma", {"1", "2", "3", "4"}, 1.0));
  for (int i = 0; i < kSmootherGammaColors; ++i) {
    params.push_back(
        Color(QStringLiteral("Gamma Color %1").arg(i + 1), 0.0));
  }
  for (size_t i = 0; i < params.size(); ++i) {
    params[i].slot = static_cast<int>(i);
  }
  info.slot_count = static_cast<int>(params.size());
  assert(info.slot_count == kSmootherSlotCount);
  assert(params[static_cast<size_t>(SmootherSlot::kGammaCount)].type ==
         FspParamType::kPopup);
  return info;
}

}  // namespace

const FspEffectInfo& OlmSmootherInfo() {
  static const FspEffectInfo info = Build();
  assert(info.slot_count == kSmootherSlotCount);
  assert(info.params.size() == static_cast<size_t>(kSmootherSlotCount));
  return info;
}

int SmootherGammaColorSlot(int index) {
  assert(index >= 0 && index < kSmootherGammaColors);
  assert(kSmootherSlotCount > kSmootherGammaColors);
  return static_cast<int>(SmootherSlot::kGammaColor0) + index;
}

}  // namespace snapper
