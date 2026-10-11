#include "ui/ui_sound.h"

#include <QAudioDevice>
#include <QDir>
#include <QFile>
#include <QMediaDevices>
#include <QSettings>
#include <QSoundEffect>
#include <QUrl>

#include <cassert>

#include "ui/blip_wave.h"

namespace snapper {
namespace {

// Sounds are quiet: they say something happened, not shout it.
constexpr double kVolume = 0.35;
constexpr double kQuiet = 0.5;

QByteArray WaveOf(UiSound::Blip blip) {
  assert(kQuiet > 0.0);
  assert(kVolume > 0.0);
  switch (blip) {
    case UiSound::Blip::kTick:
      return BlipWave(900.0, 600.0, 45, kQuiet);
    case UiSound::Blip::kChime:
      return BlipWave(520.0, 1040.0, 320, kQuiet);
  }
  return QByteArray();
}

}  // namespace

UiSound::UiSound(const QString& folder, const QString& settings_path,
                 QObject* parent)
    : QObject(parent), folder_(folder), settings_path_(settings_path) {
  assert(!folder.isEmpty());
  assert(!settings_path.isEmpty());
  const QSettings settings(settings_path_, QSettings::IniFormat);
  is_on_ = settings.value("sounds/on", true).toBool();
}

UiSound::~UiSound() = default;

void UiSound::SetOn(bool is_on) {
  assert(!settings_path_.isEmpty());
  assert(!folder_.isEmpty());
  is_on_ = is_on;
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.setValue("sounds/on", is_on_);
}

QSoundEffect* UiSound::EffectFor(Blip blip) {
  assert(!folder_.isEmpty());
  assert(blip == Blip::kTick || blip == Blip::kChime);
  std::unique_ptr<QSoundEffect>& slot =
      blip == Blip::kTick ? tick_ : chime_;
  const bool is_made = slot != nullptr;
  if (is_made) {
    return slot.get();
  }
  QDir().mkpath(folder_);
  const QString path = QDir(folder_).filePath(
      blip == Blip::kTick ? "tick.wav" : "chime.wav");
  QFile file(path);
  const bool is_written = file.open(QIODevice::WriteOnly) &&
                          file.write(WaveOf(blip)) > 0;
  if (!is_written) {
    return nullptr;
  }
  file.close();
  slot = std::make_unique<QSoundEffect>();
  slot->setSource(QUrl::fromLocalFile(path));
  slot->setVolume(static_cast<float>(kVolume));
  return slot.get();
}

void UiSound::Play(Blip blip) {
  assert(!folder_.isEmpty());
  assert(kVolume <= 1.0);
  const bool has_output = !QMediaDevices::audioOutputs().isEmpty();
  const bool can_play = is_on_ && has_output;
  if (!can_play) {
    return;
  }
  QSoundEffect* effect = EffectFor(blip);
  const bool has_effect = effect != nullptr;
  if (has_effect) {
    effect->play();
  }
}

}  // namespace snapper
