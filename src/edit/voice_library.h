#ifndef SNAPPER_EDIT_VOICE_LIBRARY_H_
#define SNAPPER_EDIT_VOICE_LIBRARY_H_

#include <QObject>
#include <QString>

#include <optional>
#include <vector>

#include "base/error.h"

namespace snapper {

// One saved voice: a sample to clone from, and what goes with it.
struct Voice final {
  QString name;
  // The folder that holds sample and voice.json.
  QString folder;
  // The sample's audio file, in the folder.
  QString sample;
  // What the sample says; empty clones from its sound alone.
  QString words;
  // The language the voice last spoke; empty for Auto.
  QString language;
  // The description a designed voice came from; empty for a cloned one.
  QString describe;
};

// The saved voices of the app, shared by every project: one folder each
// under the voices folder, holding "sample.<ext>" (the original file's
// extension kept) and "voice.json" (name, words, language, describe).
// Names are unique without regard to case. Not undoable and not part of
// a project, like the doll library.
class VoiceLibrary final : public QObject {
  Q_OBJECT

 public:
  // folder is where the voices live; made when the first is saved.
  explicit VoiceLibrary(QString folder);

  // Every voice, sorted by name.
  const std::vector<Voice>& Voices() const { return voices_; }
  std::optional<Voice> Find(const QString& name) const;
  // Saves sample_path and the rest as a voice called name. A name that
  // is taken is refused unless replace is set, which overwrites it.
  Result<void> Save(const QString& name, const QString& sample_path,
                    const QString& words, const QString& language,
                    const QString& describe, bool replace);
  Result<void> Rename(const QString& name, const QString& new_name);
  Result<void> Remove(const QString& name);

 signals:
  void Changed();

 private:
  void Reload();
  QString FolderFor(const QString& name) const;
  Result<void> Write(const Voice& voice) const;

  QString folder_;
  std::vector<Voice> voices_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_VOICE_LIBRARY_H_
