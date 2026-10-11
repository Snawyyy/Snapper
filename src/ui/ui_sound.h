#ifndef SNAPPER_UI_UI_SOUND_H_
#define SNAPPER_UI_UI_SOUND_H_

#include <QObject>
#include <QString>

#include <memory>

class QSoundEffect;

namespace snapper {

// The interface's sounds: a soft tick for undo and redo, a rising chime
// for something big going well. On and quiet by default; the choice is
// kept in the settings file. With no audio output it does nothing, and
// the sounds are made as files in a folder the first time they play.
class UiSound final : public QObject {
  Q_OBJECT

 public:
  enum class Blip { kTick, kChime };

  // folder is where the sound files are made; settings_path keeps
  // whether sounds are on.
  UiSound(const QString& folder, const QString& settings_path,
          QObject* parent);
  ~UiSound() override;

  bool is_on() const { return is_on_; }
  void SetOn(bool is_on);
  // Plays blip when sounds are on and there is somewhere to play it.
  void Play(Blip blip);

 private:
  QSoundEffect* EffectFor(Blip blip);

  QString folder_;
  QString settings_path_;
  bool is_on_ = true;
  std::unique_ptr<QSoundEffect> tick_;
  std::unique_ptr<QSoundEffect> chime_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_UI_SOUND_H_
