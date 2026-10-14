#include "ui/voice_panel.h"

#include <QSettings>
#include <QStringList>
#include <QVBoxLayout>

#include <cassert>

#include "edit/document_manager.h"
#include "edit/selection_manager.h"
#include "edit/voice_gen_manager.h"
#include "edit/voice_library.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"
#include "ui/scroll_fit.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"
#include "ui/voice_name_prompt.h"

namespace snapper {
namespace {

enum Mode { kClone = 0, kDesign = 1 };

constexpr const char* kModeKey = "voice/mode";
constexpr const char* kLanguageKey = "voice/language";
constexpr int kTextLines = 4;

// What the worker's model understands; Auto lets it tell.
QStringList Languages() {
  assert(kTextLines > 0);
  assert(kClone != kDesign);
  return {QStringLiteral("Auto"),       QStringLiteral("English"),
          QStringLiteral("Chinese"),    QStringLiteral("Japanese"),
          QStringLiteral("Korean"),     QStringLiteral("German"),
          QStringLiteral("French"),     QStringLiteral("Russian"),
          QStringLiteral("Portuguese"), QStringLiteral("Spanish"),
          QStringLiteral("Italian")};
}

}  // namespace

VoicePanel::VoicePanel(const Managers& managers)
    : managers_(managers),
      clone_(managers, &preview_),
      save_designed_(tr("Save voice...")),
      generate_(tr("Generate")),
      cancel_(tr("Cancel")),
      play_take_(tr("Play")),
      add_(tr("Add to timeline")),
      generate_key_(QKeySequence(Qt::CTRL | Qt::Key_Return), &line_),
      generate_pad_key_(QKeySequence(Qt::CTRL | Qt::Key_Enter), &line_) {
  assert(managers_.IsComplete());
  assert(managers_.voices != nullptr && managers_.voice_gen != nullptr);
  setFrameShape(QFrame::NoFrame);
  setWidgetResizable(true);
  auto* body = MakeOwned<QWidget>(this);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap);
  mode_.addItem(tr("Clone"));
  mode_.addItem(tr("Design"));
  mode_.setToolTip(tr("Clone copies the voice of a sample; Design makes a "
                      "voice from a description."));
  column->addWidget(&mode_);
  column->addWidget(&clone_);
  column->addWidget(BuildDesignPage(body));
  column->addWidget(PlatedSection(tr("Line"), BuildLineBody(body), body));
  column->addWidget(PlatedSection(tr("Result"), BuildResultBody(body), body));
  column->addStretch(1);
  setWidget(body);
  KeepWideEnough(this);
  const QSettings settings(managers_.document->SettingsPath(),
                           QSettings::IniFormat);
  ShowLanguage(settings.value(QString::fromLatin1(kLanguageKey)).toString());
  const int saved_mode =
      settings.value(QString::fromLatin1(kModeKey), kClone).toInt();
  mode_.setCurrentIndex(saved_mode == kDesign ? kDesign : kClone);
  Wire();
  ShowMode();
  Refresh();
}

QWidget* VoicePanel::BuildDesignPage(QWidget* parent) {
  assert(parent != nullptr);
  assert(design_page_ == nullptr);
  auto* body = MakeOwned<QWidget>(parent);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap / 2);
  describe_.setPlaceholderText(
      tr("A cheerful young boy, a little out of breath"));
  describe_.setFixedHeight(theme::Em(this) * kTextLines);
  describe_.setToolTip(tr("Who is speaking and how: age, mood, accent, "
                          "pace. The voice is made fresh for each line."));
  column->addWidget(&describe_);
  column->addWidget(&save_designed_);
  save_designed_.setToolTip(tr("Keeps the last designed line as a voice "
                               "you can clone from again, with the line "
                               "as its words."));
  design_page_ =
      PlatedSection(tr("Describe the voice"), body, parent);
  return design_page_;
}

QWidget* VoicePanel::BuildLineBody(QWidget* parent) {
  assert(parent != nullptr);
  assert(language_.count() == 0);
  auto* body = MakeOwned<QWidget>(parent);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap / 2);
  line_.setPlaceholderText(tr("What to say"));
  line_.setFixedHeight(theme::Em(this) * kTextLines);
  column->addWidget(&line_);
  language_.addItems(Languages());
  language_.setToolTip(tr("The language of the line. Auto lets the model "
                          "tell from the words."));
  column->addWidget(&language_);
  column->addWidget(GoldenRow(&generate_, &cancel_, body));
  cancel_.setToolTip(tr("Stops the line. The voice model has to load again "
                        "for the next one."));
  return body;
}

QWidget* VoicePanel::BuildResultBody(QWidget* parent) {
  assert(parent != nullptr);
  assert(kTextLines > 0);
  auto* body = MakeOwned<QWidget>(parent);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap / 2);
  play_take_.setCheckable(true);
  column->addWidget(GoldenRow(&take_info_, &play_take_, body));
  column->addWidget(&add_);
  return body;
}

void VoicePanel::Wire() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.voices != nullptr);
  VoiceGenManager* gen = managers_.voice_gen;
  connect(&mode_, &ChoiceRow::currentIndexChanged, this,
          &VoicePanel::ShowMode);
  connect(&language_, &QComboBox::currentIndexChanged, this, [this] {
    QSettings settings(managers_.document->SettingsPath(),
                       QSettings::IniFormat);
    settings.setValue(QString::fromLatin1(kLanguageKey), Language());
    clone_.SetLanguage(Language());
  });
  connect(&clone_, &VoiceCloneBox::Changed, this, &VoicePanel::Refresh);
  connect(&clone_, &VoiceCloneBox::LanguageSuggested, this,
          &VoicePanel::ShowLanguage);
  connect(&clone_, &VoiceCloneBox::Problem, this, &VoicePanel::Problem);
  connect(&describe_, &QPlainTextEdit::textChanged, this,
          &VoicePanel::Refresh);
  connect(&line_, &QPlainTextEdit::textChanged, this, &VoicePanel::Refresh);
  connect(&generate_, &QPushButton::clicked, this, &VoicePanel::Generate);
  connect(&generate_key_, &QShortcut::activated, this, &VoicePanel::Generate);
  connect(&generate_pad_key_, &QShortcut::activated, this,
          &VoicePanel::Generate);
  connect(&cancel_, &QPushButton::clicked, gen, &VoiceGenManager::Cancel);
  connect(&play_take_, &QPushButton::clicked, this,
          [this] { preview_.Toggle(managers_.voice_gen->last_take()); });
  connect(&add_, &QPushButton::clicked, this, &VoicePanel::AddToTimeline);
  connect(&save_designed_, &QPushButton::clicked, this,
          &VoicePanel::SaveDesigned);
  connect(&preview_, &AudioPreview::Changed, this, &VoicePanel::Refresh);
  connect(gen, &VoiceGenManager::StateChanged, this, &VoicePanel::Refresh);
  connect(gen, &VoiceGenManager::TakeReady, this, &VoicePanel::Refresh);
  connect(gen, &VoiceGenManager::Problem, this, &VoicePanel::Problem);
  generate_key_.setContext(Qt::WidgetShortcut);
  generate_pad_key_.setContext(Qt::WidgetShortcut);
}

bool VoicePanel::IsDesign() const {
  assert(mode_.count() == 2);
  assert(kClone != kDesign);
  return mode_.currentIndex() == kDesign;
}

QString VoicePanel::Language() const {
  assert(language_.count() >= 0);
  assert(kTextLines > 0);
  return language_.currentText();
}

void VoicePanel::ShowLanguage(const QString& language) {
  assert(language.size() < 1000);
  assert(language_.count() > 0);
  const int found = language_.findText(language, Qt::MatchFixedString);
  language_.setCurrentIndex(found >= 0 ? found : 0);
  clone_.SetLanguage(Language());
}

void VoicePanel::ShowMode() {
  assert(design_page_ != nullptr);
  assert(mode_.count() == 2);
  const bool is_design = IsDesign();
  design_page_->setVisible(is_design);
  clone_.setVisible(!is_design);
  QSettings settings(managers_.document->SettingsPath(),
                     QSettings::IniFormat);
  settings.setValue(QString::fromLatin1(kModeKey),
                    is_design ? kDesign : kClone);
  Refresh();
}

void VoicePanel::Generate() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.voices != nullptr);
  VoiceGenManager& gen = *managers_.voice_gen;
  preview_.Stop();
  const QString text = line_.toPlainText();
  const auto started =
      IsDesign() ? gen.SpeakDesign(describe_.toPlainText(), text, Language())
                 : gen.SpeakClone(clone_.sample_path(), clone_.words(), text,
                                  Language());
  Report(ProblemOf(started));
}

void VoicePanel::AddToTimeline() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.selection != nullptr);
  preview_.Stop();
  const auto added = managers_.voice_gen->AddToReel();
  Report(ProblemOf(added));
  if (!added) {
    return;
  }
  // The new clip is picked, so it shows where it landed.
  managers_.selection->PickClips({*added}, PickMode::kReplace);
  managers_.toasts->Done(tr("Added the line to the reel."));
}

void VoicePanel::SaveDesigned() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.voices != nullptr);
  const auto chosen = AskVoiceName(this, *managers_.voices, QString(), true);
  if (!chosen) {
    return;
  }
  const auto saved =
      managers_.voice_gen->SaveDesigned(chosen->name, chosen->replace);
  Report(ProblemOf(saved));
  if (saved) {
    managers_.toasts->Say(tr("Saved the voice \"%1\".").arg(chosen->name));
  }
}

void VoicePanel::Refresh() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.toasts != nullptr);
  const VoiceGenManager& gen = *managers_.voice_gen;
  using State = VoiceGenManager::State;
  const State state = gen.state();
  generate_.setText(state == State::kLoading    ? tr("Loading voice model...")
                    : state == State::kSpeaking ? tr("Speaking...")
                                                : tr("Generate"));
  cancel_.setVisible(gen.IsBusy());
  const QString text = line_.toPlainText();
  const QString why_not =
      IsDesign() ? gen.WhyNoSpeakDesign(describe_.toPlainText(), text)
                 : gen.WhyNoSpeakClone(clone_.sample_path(), text);
  Explain(&generate_, why_not,
          tr("Says the line in this voice (Ctrl+Enter in the line). The "
             "first line takes a while, as the voice model loads."));
  Explain(&save_designed_, gen.WhyNoSaveDesigned());
  const QString no_take = gen.WhyNoAdd();
  const bool has_take = no_take.isEmpty();
  const bool is_playing = preview_.IsPlaying(gen.last_take());
  play_take_.setChecked(is_playing);
  play_take_.setText(is_playing ? tr("Stop") : tr("Play"));
  Explain(&play_take_, no_take);
  Explain(&add_, no_take,
          tr("Lays the line on a sound track at the playhead and picks it."));
  take_info_.setText(has_take ? tr("%1 s, %2")
                                    .arg(gen.last_seconds(), 0, 'f', 1)
                                    .arg(gen.last_voice_name())
                              : tr("No line yet."));
}

}  // namespace snapper
