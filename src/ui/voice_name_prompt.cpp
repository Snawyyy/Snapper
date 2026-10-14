#include "ui/voice_name_prompt.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

#include <cassert>

#include "edit/voice_library.h"

namespace snapper {

std::optional<VoiceName> AskVoiceName(QWidget* parent,
                                      const VoiceLibrary& library,
                                      const QString& initial,
                                      bool may_replace) {
  assert(parent != nullptr);
  assert(initial.size() < 10000);
  bool is_ok = false;
  const QString asked = QInputDialog::getText(
      parent, QObject::tr("Save voice"), QObject::tr("Name the voice"),
      QLineEdit::Normal, initial, &is_ok);
  const QString name = asked.trimmed();
  const bool is_named = is_ok && !name.isEmpty();
  if (!is_named) {
    return std::nullopt;
  }
  const bool is_own = name.compare(initial, Qt::CaseInsensitive) == 0;
  const bool is_taken = library.Find(name).has_value() &&
                        (may_replace || !is_own);
  if (!is_taken) {
    return VoiceName{name, false};
  }
  if (!may_replace) {
    QMessageBox::warning(
        parent, QObject::tr("Rename voice"),
        QObject::tr("A voice called \"%1\" is already saved.").arg(name));
    return std::nullopt;
  }
  const auto answer = QMessageBox::question(
      parent, QObject::tr("Replace voice"),
      QObject::tr("A voice called \"%1\" is already saved. Replace it?")
          .arg(name));
  const bool is_replacing = answer == QMessageBox::Yes;
  return is_replacing ? std::optional<VoiceName>(VoiceName{name, true})
                      : std::nullopt;
}

}  // namespace snapper
