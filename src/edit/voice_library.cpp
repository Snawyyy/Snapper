#include "edit/voice_library.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>
#include <cassert>
#include <utility>

#include "base/text.h"

namespace snapper {
namespace {

constexpr int kMaxVoices = 1000;
constexpr const char* kSampleName = "sample";
constexpr const char* kVoiceFile = "voice.json";

// A folder name made from a voice name: letters, digits, spaces, dashes
// and underscores stay, everything else becomes an underscore.
QString SafeName(const QString& name) {
  assert(kMaxVoices > 0);
  assert(!name.isEmpty());
  QString safe;
  for (const QChar letter : name.trimmed()) {
    const bool is_plain = letter.isLetterOrNumber() || letter == ' ' ||
                          letter == '-' || letter == '_';
    safe.append(is_plain ? letter : QChar('_'));
  }
  const bool is_dotted = safe.startsWith('.');
  return is_dotted ? QStringLiteral("_") + safe : safe;
}

QStringList SampleFiles(const QDir& folder) {
  assert(!folder.path().isEmpty());
  assert(kSampleName != nullptr);
  return folder.entryList({QString::fromLatin1(kSampleName) + ".*"},
                          QDir::Files);
}

std::optional<Voice> ReadVoice(const QString& folder_path) {
  assert(!folder_path.isEmpty());
  assert(kVoiceFile != nullptr);
  const QDir folder(folder_path);
  QFile file(folder.filePath(QString::fromLatin1(kVoiceFile)));
  const bool is_open = file.open(QIODevice::ReadOnly);
  const QStringList samples = SampleFiles(folder);
  const bool is_voice = is_open && !samples.isEmpty();
  if (!is_voice) {
    return std::nullopt;
  }
  const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
  Voice voice;
  voice.name = json.value("name").toString();
  voice.folder = folder.absolutePath();
  voice.sample = folder.filePath(samples.front());
  voice.words = json.value("words").toString();
  voice.language = json.value("language").toString();
  voice.describe = json.value("describe").toString();
  const bool is_named = !voice.name.isEmpty();
  return is_named ? std::optional<Voice>(voice) : std::nullopt;
}

bool SameName(const QString& first, const QString& second) {
  assert(kMaxVoices > 0);
  assert(kVoiceFile != nullptr);
  return first.compare(second, Qt::CaseInsensitive) == 0;
}

}  // namespace

VoiceLibrary::VoiceLibrary(QString folder) : folder_(std::move(folder)) {
  assert(!folder_.isEmpty());
  assert(voices_.empty());
  Reload();
}

void VoiceLibrary::Reload() {
  assert(!folder_.isEmpty());
  assert(kMaxVoices > 0);
  voices_.clear();
  const QDir root(folder_);
  const QStringList names = root.entryList(
      QDir::Dirs | QDir::NoDotAndDotDot);
  const int count = std::min(static_cast<int>(names.size()), kMaxVoices);
  for (int i = 0; i < count; ++i) {
    const auto voice = ReadVoice(root.filePath(names.at(i)));
    if (voice) {
      voices_.push_back(*voice);
    }
  }
  std::sort(voices_.begin(), voices_.end(),
            [](const Voice& first, const Voice& second) {
              return first.name.compare(second.name, Qt::CaseInsensitive) < 0;
            });
}

std::optional<Voice> VoiceLibrary::Find(const QString& name) const {
  assert(voices_.size() <= static_cast<size_t>(kMaxVoices));
  assert(!folder_.isEmpty());
  for (const Voice& voice : voices_) {
    const bool is_match = SameName(voice.name, name);
    if (is_match) {
      return voice;
    }
  }
  return std::nullopt;
}

QString VoiceLibrary::FolderFor(const QString& name) const {
  assert(!name.trimmed().isEmpty());
  assert(kMaxVoices > 0);
  const auto existing = Find(name);
  if (existing) {
    return existing->folder;
  }
  const QDir root(folder_);
  const QString base = SafeName(name);
  QString candidate = base;
  for (int number = 2; number <= kMaxVoices; ++number) {
    const bool is_free = !root.exists(candidate);
    if (is_free) {
      break;
    }
    candidate = QStringLiteral("%1 %2").arg(base).arg(number);
  }
  return root.filePath(candidate);
}

Result<void> VoiceLibrary::Write(const Voice& voice) const {
  assert(!voice.name.isEmpty());
  assert(!voice.folder.isEmpty());
  const QJsonObject json{{"name", voice.name},
                         {"words", voice.words},
                         {"language", voice.language},
                         {"describe", voice.describe}};
  QSaveFile file(QDir(voice.folder).filePath(QString::fromLatin1(kVoiceFile)));
  const bool is_written =
      file.open(QIODevice::WriteOnly) &&
      file.write(QJsonDocument(json).toJson()) > 0 && file.commit();
  if (!is_written) {
    return std::unexpected(Error{Tr("Could not save the voice to %1.")
                                     .arg(QDir::toNativeSeparators(
                                         voice.folder))});
  }
  return {};
}

Result<void> VoiceLibrary::Save(const QString& name,
                                const QString& sample_path,
                                const QString& words, const QString& language,
                                const QString& describe, bool replace) {
  assert(!folder_.isEmpty());
  assert(kMaxVoices > 0);
  const QString trimmed = name.trimmed();
  const bool is_unnamed = trimmed.isEmpty();
  if (is_unnamed) {
    return std::unexpected(Error{Tr("Name the voice first.")});
  }
  const bool has_sample = QFileInfo(sample_path).isFile();
  if (!has_sample) {
    return std::unexpected(Error{Tr("The voice's sample file is missing.")});
  }
  const bool is_taken = Find(trimmed).has_value();
  const bool is_refused = is_taken && !replace;
  if (is_refused) {
    return std::unexpected(Error{
        Tr("A voice called \"%1\" is already saved.").arg(trimmed)});
  }
  Voice voice{trimmed, FolderFor(trimmed), QString(), words, language,
              describe};
  const QDir folder(voice.folder);
  const bool has_folder = QDir().mkpath(voice.folder);
  voice.sample = folder.filePath(QString::fromLatin1(kSampleName) + "." +
                                 QFileInfo(sample_path).suffix().toLower());
  const bool is_same_file =
      QFileInfo(sample_path).canonicalFilePath() ==
      QFileInfo(voice.sample).canonicalFilePath();
  const bool is_replacing = !is_same_file && has_folder;
  if (is_replacing) {
    for (const QString& old : SampleFiles(folder)) {
      QFile::remove(folder.filePath(old));
    }
  }
  const bool is_copied =
      has_folder && (is_same_file || QFile::copy(sample_path, voice.sample));
  if (!is_copied) {
    return std::unexpected(Error{Tr("Could not copy the sample to %1.")
                                     .arg(QDir::toNativeSeparators(
                                         voice.folder))});
  }
  const auto written = Write(voice);
  if (!written) {
    return written;
  }
  Reload();
  emit Changed();
  return {};
}

Result<void> VoiceLibrary::Rename(const QString& name,
                                  const QString& new_name) {
  assert(!folder_.isEmpty());
  assert(kMaxVoices > 0);
  auto voice = Find(name);
  if (!voice) {
    return std::unexpected(Error{Tr("That voice is not saved.")});
  }
  const QString trimmed = new_name.trimmed();
  const bool is_unnamed = trimmed.isEmpty();
  if (is_unnamed) {
    return std::unexpected(Error{Tr("Name the voice first.")});
  }
  const bool is_taken = !SameName(name, trimmed) && Find(trimmed).has_value();
  if (is_taken) {
    return std::unexpected(Error{
        Tr("A voice called \"%1\" is already saved.").arg(trimmed)});
  }
  voice->name = trimmed;
  const auto written = Write(*voice);
  if (!written) {
    return written;
  }
  Reload();
  emit Changed();
  return {};
}

Result<void> VoiceLibrary::Remove(const QString& name) {
  assert(!folder_.isEmpty());
  assert(kMaxVoices > 0);
  const auto voice = Find(name);
  if (!voice) {
    return std::unexpected(Error{Tr("That voice is not saved.")});
  }
  const bool is_removed = QDir(voice->folder).removeRecursively();
  if (!is_removed) {
    return std::unexpected(Error{Tr("Could not remove the voice's folder %1.")
                                     .arg(QDir::toNativeSeparators(
                                         voice->folder))});
  }
  Reload();
  emit Changed();
  return {};
}

}  // namespace snapper
