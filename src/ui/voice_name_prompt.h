#ifndef SNAPPER_UI_VOICE_NAME_PROMPT_H_
#define SNAPPER_UI_VOICE_NAME_PROMPT_H_

#include <QString>
#include <QWidget>

#include <optional>

namespace snapper {

class VoiceLibrary;

// What the user chose when saving a voice.
struct VoiceName final {
  QString name;
  // The name is taken and the user agreed to replace that voice.
  bool replace = false;
};

// Asks what to call a voice, starting from initial. A name already
// saved is asked about when may_replace (the voice is being saved), else
// refused with a note (the voice is being renamed; its own name is
// fine). Empty when the user gave up or left the name blank.
std::optional<VoiceName> AskVoiceName(QWidget* parent,
                                      const VoiceLibrary& library,
                                      const QString& initial,
                                      bool may_replace);

}  // namespace snapper

#endif  // SNAPPER_UI_VOICE_NAME_PROMPT_H_
