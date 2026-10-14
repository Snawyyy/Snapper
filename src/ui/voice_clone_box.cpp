#include "ui/voice_clone_box.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStyle>

#include <cassert>

#include "edit/voice_gen_manager.h"
#include "edit/voice_library.h"
#include "media/desktop_recorder.h"
#include "ui/audio_preview.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"
#include "ui/theme.h"
#include "ui/voice_name_prompt.h"

namespace snapper {
namespace {

// A sample outside these lengths clones worse.
constexpr double kShortSample = 3.0;
constexpr double kLongSample = 20.0;

}  // namespace

VoiceCloneBox::VoiceCloneBox(const Managers& managers, AudioPreview* preview)
    : managers_(managers),
      preview_(preview),
      layout_(this),
      rename_(tr("Rename...")),
      delete_(tr("Delete")),
      open_(tr("Open file...")),
      record_(tr("Rec PC audio")),
      play_(tr("Play")),
      save_(tr("Save voice...")),
      screen_(preview, tr("NO SAMPLE"), nullptr),
      meter_(nullptr) {
  assert(managers_.voices != nullptr && managers_.voice_gen != nullptr);
  assert(preview_ != nullptr);
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(theme::kRowGap);
  layout_.addWidget(PlatedSection(tr("1  Voice"), BuildVoiceBody(), this));
  layout_.addWidget(
      PlatedSection(tr("2  Sample"), BuildSampleBody(), this));
  connect(&voices_box_, &QComboBox::activated, this,
          &VoiceCloneBox::LoadPickedVoice);
  connect(&rename_, &QPushButton::clicked, this, &VoiceCloneBox::RenameVoice);
  connect(&delete_, &QPushButton::clicked, this, &VoiceCloneBox::DeleteVoice);
  connect(&open_, &QPushButton::clicked, this, &VoiceCloneBox::AskForSample);
  connect(&record_, &QPushButton::clicked, this,
          &VoiceCloneBox::ToggleCapture);
  connect(&play_, &QPushButton::clicked, this,
          [this] { preview_->Toggle(sample_path_); });
  connect(&save_, &QPushButton::clicked, this, &VoiceCloneBox::SaveVoice);
  connect(preview_, &AudioPreview::Changed, this, &VoiceCloneBox::Refresh);
  VoiceGenManager* gen = managers_.voice_gen;
  connect(gen, &VoiceGenManager::StateChanged, this,
          &VoiceCloneBox::Refresh);
  connect(gen, &VoiceGenManager::CaptureLevel, &meter_, &LedMeter::SetLevel);
  connect(gen, &VoiceGenManager::CaptureSeconds, this, [this](double seconds) {
    capture_seconds_ = seconds;
    meter_.SetSeconds(seconds, kMaxSampleSeconds);
    Refresh();
  });
  connect(gen, &VoiceGenManager::SampleCaptured, this,
          &VoiceCloneBox::UseNewSample);
  connect(managers_.voices, &VoiceLibrary::Changed, this,
          [this] { Refill(PickedVoice()); });
  Refill(QString());
  Refresh();
}

QWidget* VoiceCloneBox::BuildVoiceBody() {
  assert(layout_.count() == 0);
  assert(managers_.voices != nullptr);
  auto* body = MakeOwned<QWidget>(this);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  voices_box_.setToolTip(tr("The voices saved on this computer, for every "
                            "project. New voice is a sample not saved yet."));
  rename_.setToolTip(tr("Gives the picked voice another name."));
  delete_.setToolTip(tr("Removes the picked voice and its sample file."));
  column->addWidget(GoldenRow(
      &voices_box_, SharedRow({&rename_, &delete_}, {}, kTileGap, body),
      body));
  return body;
}

QWidget* VoiceCloneBox::BuildSampleBody() {
  assert(layout_.count() == 1);
  assert(preview_ != nullptr);
  auto* body = MakeOwned<QWidget>(this);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap / 2);
  // The meter takes the screen's place while recording, at its height,
  // so nothing under them moves.
  column->addWidget(&screen_);
  column->addWidget(&meter_);
  meter_.setFixedHeight(screen_.minimumHeight());
  screen_.setFixedHeight(screen_.minimumHeight());
  record_.setCheckable(true);
  play_.setCheckable(true);
  open_.setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
  save_.setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
  column->addWidget(TileRow({&open_, &record_, &play_}, body));
  open_.setToolTip(tr("Takes a sound file (wav, mp3, flac or ogg) as the "
                      "sample to clone."));
  hint_ = EmptyHint(tr("3 to 15 seconds of clear speech works best."), body);
  column->addWidget(hint_);
  column->addWidget(MakeOwned<QLabel>(tr("Words in the sample"), body));
  words_.setPlaceholderText(
      tr("Type what the sample says for a closer clone (optional)"));
  words_.setFixedHeight(theme::Em(this) * 4);
  words_.setToolTip(tr("What is said in the sample, as written. With it the "
                       "clone is closer; without it only the sound of the "
                       "voice is copied."));
  column->addWidget(&words_);
  column->addWidget(&save_);
  return body;
}

void VoiceCloneBox::Refill(const QString& pick) {
  assert(managers_.voices != nullptr);
  assert(pick.size() < 10000);
  const bool was_blocked = voices_box_.blockSignals(true);
  voices_box_.clear();
  voices_box_.addItem(tr("New voice"), QString());
  for (const Voice& voice : managers_.voices->Voices()) {
    voices_box_.addItem(voice.name, voice.name);
  }
  const int found = pick.isEmpty() ? -1 : voices_box_.findData(pick);
  voices_box_.setCurrentIndex(found > 0 ? found : 0);
  voices_box_.blockSignals(was_blocked);
  Refresh();
}

QString VoiceCloneBox::PickedVoice() const {
  assert(voices_box_.count() >= 0);
  assert(managers_.voices != nullptr);
  const QString name = voices_box_.currentData().toString();
  return name;
}

void VoiceCloneBox::LoadPickedVoice() {
  assert(managers_.voices != nullptr);
  assert(managers_.voice_gen != nullptr);
  const auto voice = managers_.voices->Find(PickedVoice());
  if (!voice) {
    return;
  }
  words_.setPlainText(voice->words);
  Adopt(voice->sample);
  const bool has_language = !voice->language.isEmpty();
  if (has_language) {
    emit LanguageSuggested(voice->language);
  }
}

void VoiceCloneBox::SetLanguage(const QString& language) {
  assert(language.size() < 1000);
  assert(managers_.voices != nullptr);
  language_ = language;
}

void VoiceCloneBox::AskForSample() {
  assert(managers_.voice_gen != nullptr);
  assert(preview_ != nullptr);
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Open a voice sample"), QString(),
      tr("Sound (*.wav *.mp3 *.flac *.ogg);;All files (*)"));
  const bool is_chosen = !path.isEmpty();
  if (is_chosen) {
    UseNewSample(path);
  }
}

void VoiceCloneBox::UseNewSample(const QString& path) {
  assert(!path.isEmpty());
  assert(managers_.voices != nullptr);
  // The words of a saved voice belong to its own sample.
  const bool was_saved = !PickedVoice().isEmpty();
  if (was_saved) {
    words_.clear();
  }
  Refill(QString());
  Adopt(path);
}

void VoiceCloneBox::Adopt(const QString& path) {
  assert(!path.isEmpty());
  assert(managers_.voice_gen != nullptr);
  const auto seconds = managers_.voice_gen->SampleSeconds(path);
  if (!seconds) {
    Report(seconds.error().message);
    return;
  }
  preview_->Stop();
  sample_path_ = path;
  sample_seconds_ = *seconds;
  Refresh();
  emit Changed();
}

void VoiceCloneBox::ToggleCapture() {
  assert(managers_.voice_gen != nullptr);
  assert(kMaxSampleSeconds > 0);
  preview_->Stop();
  capture_seconds_ = 0.0;
  meter_.SetSeconds(0.0, kMaxSampleSeconds);
  Report(ProblemOf(managers_.voice_gen->ToggleCapture()));
  Refresh();
}

void VoiceCloneBox::SaveVoice() {
  assert(managers_.voices != nullptr);
  assert(!sample_path_.isEmpty());
  const auto chosen =
      AskVoiceName(this, *managers_.voices, PickedVoice(), true);
  if (!chosen) {
    return;
  }
  const auto saved = managers_.voices->Save(
      chosen->name, sample_path_, words(), language_, QString(),
      chosen->replace);
  Report(ProblemOf(saved));
  if (saved) {
    Refill(chosen->name);
    Adopt(managers_.voices->Find(chosen->name)->sample);
  }
}

void VoiceCloneBox::RenameVoice() {
  assert(managers_.voices != nullptr);
  assert(!PickedVoice().isEmpty());
  const QString old_name = PickedVoice();
  const auto chosen =
      AskVoiceName(this, *managers_.voices, old_name, false);
  const bool is_new_name = chosen && chosen->name != old_name;
  if (!is_new_name) {
    return;
  }
  const auto renamed = managers_.voices->Rename(old_name, chosen->name);
  Report(ProblemOf(renamed));
  if (renamed) {
    Refill(chosen->name);
  }
}

void VoiceCloneBox::DeleteVoice() {
  assert(managers_.voices != nullptr);
  assert(!PickedVoice().isEmpty());
  const QString name = PickedVoice();
  const QString folder = managers_.voices->Find(name)->folder;
  const auto answer = QMessageBox::question(
      this, tr("Delete voice"),
      tr("Delete the voice \"%1\" and its sample file?").arg(name));
  const bool is_confirmed = answer == QMessageBox::Yes;
  if (!is_confirmed) {
    return;
  }
  preview_->Stop();
  const auto removed = managers_.voices->Remove(name);
  Report(ProblemOf(removed));
  // The deleted voice's sample file is gone, so it can't stay the sample.
  const bool is_sample_gone = removed && sample_path_.startsWith(folder);
  if (is_sample_gone) {
    sample_path_.clear();
    sample_seconds_ = 0.0;
    words_.clear();
    Refresh();
    emit Changed();
  }
}

void VoiceCloneBox::Refresh() {
  assert(managers_.voice_gen != nullptr);
  assert(preview_ != nullptr);
  const VoiceGenManager& gen = *managers_.voice_gen;
  const bool is_capturing = gen.IsCapturing();
  record_.setChecked(is_capturing);
  record_.setText(is_capturing ? tr("Stop") : tr("Rec PC"));
  record_.setIcon(style()->standardIcon(
      is_capturing ? QStyle::SP_MediaStop : QStyle::SP_MediaVolume));
  Explain(&record_, gen.WhyNoCapture(),
          tr("Records what the PC plays, up to %1 seconds, as the sample. "
             "Play a clip of the voice, press this, press it again.")
              .arg(kMaxSampleSeconds));
  const bool was_shown = !meter_.isHidden();
  meter_.setVisible(is_capturing);
  screen_.setVisible(!is_capturing);
  const bool is_new_state = was_shown != is_capturing;
  if (is_new_state) {
    meter_.SetRunning(is_capturing);
  }
  const bool has_sample = !sample_path_.isEmpty();
  const QString no_sample = tr("Open or record a sample first.");
  screen_.SetFile(sample_path_, PickedVoice().isEmpty() ? tr("Sample")
                                                        : PickedVoice());
  const bool is_playing = preview_->IsPlaying(sample_path_);
  play_.setChecked(is_playing);
  play_.setText(is_playing ? tr("Stop") : tr("Play"));
  play_.setIcon(style()->standardIcon(
      is_playing ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
  Explain(&play_, has_sample ? QString() : no_sample);
  Explain(&save_, has_sample ? QString() : no_sample,
          tr("Keeps the sample and its words as a voice to pick again "
             "later, in any project."));
  const bool is_poor = has_sample && (sample_seconds_ < kShortSample ||
                                      sample_seconds_ > kLongSample);
  hint_->setVisible(is_poor);
  const bool has_voice = !PickedVoice().isEmpty();
  const QString no_voice = tr("Pick a saved voice first.");
  Explain(&rename_, has_voice ? QString() : no_voice);
  Explain(&delete_, has_voice ? QString() : no_voice);
}

}  // namespace snapper
