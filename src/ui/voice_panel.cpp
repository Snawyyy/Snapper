#include "ui/voice_panel.h"

#include <QSettings>
#include <QStringList>
#include <QStyle>
#include <QVBoxLayout>

#include <cassert>

#include "edit/document_manager.h"
#include "edit/selection_manager.h"
#include "edit/voice_gen_manager.h"
#include "edit/voice_library.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/pose_controls.h"
#include "ui/problem.h"
#include "ui/scroll_fit.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"
#include "ui/voice_name_prompt.h"

namespace snapper {
namespace {

constexpr const char* kModeKey = "voice/mode";
constexpr const char* kLanguageKey = "voice/language";
constexpr int kTextLines = 4;
constexpr int kDesignMode = 1;

// What the worker's model understands; Auto lets it tell.
QStringList Languages() {
  assert(kTextLines > 0);
  assert(kDesignMode != 0);
  return {QStringLiteral("Auto"),       QStringLiteral("English"),
          QStringLiteral("Chinese"),    QStringLiteral("Japanese"),
          QStringLiteral("Korean"),     QStringLiteral("German"),
          QStringLiteral("French"),     QStringLiteral("Russian"),
          QStringLiteral("Portuguese"), QStringLiteral("Spanish"),
          QStringLiteral("Italian")};
}

// Renames the title plate of a section PlatedSection made.
void Retitle(QWidget* section, const QString& title) {
  assert(section != nullptr);
  assert(!title.isEmpty());
  auto* plate = section->findChild<QLabel*>(QStringLiteral("plate"));
  const bool has_plate = plate != nullptr;
  if (has_plate) {
    plate->setText(title);
  }
}

}  // namespace

VoicePanel::VoicePanel(const Managers& managers)
    : managers_(managers),
      clone_(managers, &preview_),
      engine_box_(managers),
      save_designed_(tr("Save voice...")),
      generate_(tr("Generate"), nullptr),
      take_screen_(&preview_, tr("NO LINE YET"), nullptr),
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
  column->addWidget(BuildModeRow(body));
  column->addWidget(&clone_);
  column->addWidget(BuildDesignPage(body));
  line_section_ = PlatedSection(tr("Line"), BuildLineBody(body), body);
  column->addWidget(line_section_);
  result_section_ =
      PlatedSection(tr("Listen"), BuildResultBody(body), body);
  column->addWidget(result_section_);
  column->addStretch(1);
  setWidget(body);
  KeepWideEnough(this);
  const QSettings settings(managers_.document->SettingsPath(),
                           QSettings::IniFormat);
  ShowLanguage(settings.value(QString::fromLatin1(kLanguageKey)).toString());
  is_design_ =
      settings.value(QString::fromLatin1(kModeKey), 0).toInt() == kDesignMode;
  Wire();
  ShowMode();
}

QWidget* VoicePanel::BuildModeRow(QWidget* parent) {
  assert(parent != nullptr);
  assert(clone_mode_.text().isEmpty());
  auto* row = MakeOwned<QWidget>(parent);
  auto* column = MakeOwned<QVBoxLayout>(row);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap / 2);
  clone_mode_.setText(tr("Clone"));
  design_mode_.setText(tr("Design"));
  for (QPushButton* mode : {&clone_mode_, &design_mode_}) {
    mode->setCheckable(true);
    mode->setMinimumHeight(theme::Em(this) * 2);
  }
  clone_mode_.setToolTip(tr("Copy the voice in a sample."));
  design_mode_.setToolTip(tr("Make up a voice from a description."));
  column->addWidget(TileRow({&clone_mode_, &design_mode_}, row));
  mode_caption_.setAlignment(Qt::AlignCenter);
  mode_caption_.setWordWrap(true);
  column->addWidget(&mode_caption_);
  return row;
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
  save_designed_.setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
  column->addWidget(&save_designed_);
  design_page_ = PlatedSection(tr("1  Describe the voice"), body, parent);
  return design_page_;
}

QWidget* VoicePanel::BuildLineBody(QWidget* parent) {
  assert(parent != nullptr);
  assert(language_.count() == 0);
  auto* body = MakeOwned<QWidget>(parent);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap / 2);
  line_.setPlaceholderText(tr("What should they say?"));
  line_.setFixedHeight(theme::Em(this) * kTextLines);
  column->addWidget(&line_);
  column->addWidget(&engine_box_);
  language_.addItems(Languages());
  language_.setToolTip(tr("The language of the line. Auto lets the model "
                          "tell from the words."));
  column->addWidget(MakeNamedRow(tr("Language"), &language_, body));
  column->addWidget(&generate_);
  return body;
}

QWidget* VoicePanel::BuildResultBody(QWidget* parent) {
  assert(parent != nullptr);
  assert(kTextLines > 0);
  auto* body = MakeOwned<QWidget>(parent);
  auto* column = MakeOwned<QVBoxLayout>(body);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(theme::kRowGap / 2);
  column->addWidget(&take_screen_);
  play_take_.setCheckable(true);
  add_.setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
  column->addWidget(GoldenRow(&add_, &play_take_, body));
  return body;
}

void VoicePanel::Wire() {
  assert(managers_.voice_gen != nullptr);
  assert(clone_mode_.isCheckable() && design_mode_.isCheckable());
  VoiceGenManager* gen = managers_.voice_gen;
  connect(&clone_mode_, &QPushButton::clicked, this,
          [this] { SetMode(false); });
  connect(&design_mode_, &QPushButton::clicked, this,
          [this] { SetMode(true); });
  connect(&language_, &QComboBox::currentIndexChanged, this, [this] {
    QSettings settings(managers_.document->SettingsPath(),
                       QSettings::IniFormat);
    settings.setValue(QString::fromLatin1(kLanguageKey), Language());
    clone_.SetLanguage(Language());
  });
  connect(&clone_, &VoiceCloneBox::Changed, this, &VoicePanel::Refresh);
  connect(&engine_box_, &VoiceEngineBox::Changed, this,
          &VoicePanel::Refresh);
  connect(&clone_, &VoiceCloneBox::LanguageSuggested, this,
          &VoicePanel::ShowLanguage);
  connect(&clone_, &VoiceCloneBox::Problem, this, &VoicePanel::Problem);
  connect(&describe_, &QPlainTextEdit::textChanged, this,
          &VoicePanel::Refresh);
  connect(&line_, &QPlainTextEdit::textChanged, this, &VoicePanel::Refresh);
  connect(&generate_, &QPushButton::clicked, this,
          &VoicePanel::GenerateOrStop);
  connect(&generate_key_, &QShortcut::activated, this,
          &VoicePanel::GenerateOrStop);
  connect(&generate_pad_key_, &QShortcut::activated, this,
          &VoicePanel::GenerateOrStop);
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

void VoicePanel::SetMode(bool is_design) {
  assert(managers_.document != nullptr);
  assert(design_page_ != nullptr);
  is_design_ = is_design;
  QSettings settings(managers_.document->SettingsPath(),
                     QSettings::IniFormat);
  settings.setValue(QString::fromLatin1(kModeKey),
                    is_design ? kDesignMode : 0);
  ShowMode();
}

void VoicePanel::ShowMode() {
  assert(design_page_ != nullptr && line_section_ != nullptr);
  assert(result_section_ != nullptr);
  const bool is_design = IsDesign();
  design_page_->setVisible(is_design);
  clone_.setVisible(!is_design);
  // Designs are Qwen's only, so the model and emotion are clone settings.
  engine_box_.setVisible(!is_design);
  clone_mode_.setChecked(!is_design);
  design_mode_.setChecked(is_design);
  mode_caption_.setText(
      is_design ? tr("Describe a voice in words and hear it speak.")
                : tr("Give it a few seconds of someone talking, and it "
                     "says anything in their voice."));
  // The steps count on from the voice's own: two for a clone, one for
  // a design.
  const int first = is_design ? 2 : 3;
  Retitle(line_section_, tr("%1  Line").arg(first));
  Retitle(result_section_, tr("%1  Listen").arg(first + 1));
  Refresh();
}

void VoicePanel::GenerateOrStop() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.voices != nullptr);
  VoiceGenManager& gen = *managers_.voice_gen;
  const bool is_busy = gen.IsBusy();
  if (is_busy) {
    gen.Cancel();
    return;
  }
  preview_.Stop();
  const QString text = line_.toPlainText();
  const auto started =
      IsDesign() ? gen.SpeakDesign(describe_.toPlainText(), text, Language())
                 : gen.SpeakClone(clone_.sample_path(), clone_.words(), text,
                                  Language(), engine_box_.engine(),
                                  engine_box_.emotion());
  Report(ProblemOf(started));
}

void VoicePanel::AddToTimeline() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.selection != nullptr);
  preview_.Stop();
  const auto added = managers_.voice_gen->AddToReel();
  Report(ProblemOf(added));
  const bool is_added = added.has_value();
  if (!is_added) {
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
  const bool is_chosen = chosen.has_value();
  if (!is_chosen) {
    return;
  }
  const auto saved =
      managers_.voice_gen->SaveDesigned(chosen->name, chosen->replace);
  Report(ProblemOf(saved));
  const bool is_saved = saved.has_value();
  if (is_saved) {
    managers_.toasts->Say(tr("Saved the voice \"%1\".").arg(chosen->name));
  }
}

void VoicePanel::ShowGenerate() {
  assert(managers_.voice_gen != nullptr);
  assert(design_page_ != nullptr);
  const VoiceGenManager& gen = *managers_.voice_gen;
  using State = VoiceGenManager::State;
  const State state = gen.state();
  const bool is_busy = gen.IsBusy();
  generate_.SetBusy(is_busy);
  if (is_busy) {
    generate_.setText(state == State::kLoading     ? tr("Loading voice...")
                      : state == State::kListening ? tr("Listening...")
                                                   : tr("Speaking..."));
    generate_.setEnabled(true);
    generate_.setToolTip(tr("Click to stop. The voice model has to load "
                            "again for the next line."));
    return;
  }
  generate_.setText(tr("Generate"));
  const QString text = line_.toPlainText();
  const QString why_not =
      IsDesign() ? gen.WhyNoSpeakDesign(describe_.toPlainText(), text)
                 : gen.WhyNoSpeakClone(clone_.sample_path(), text,
                                       engine_box_.engine(), Language());
  Explain(&generate_, why_not,
          tr("Says the line in this voice (Ctrl+Enter in the line). The "
             "first line takes a while, as the voice model loads."));
}

void VoicePanel::Refresh() {
  assert(managers_.voice_gen != nullptr);
  assert(managers_.toasts != nullptr);
  const VoiceGenManager& gen = *managers_.voice_gen;
  ShowGenerate();
  engine_box_.Refresh();
  Explain(&save_designed_, gen.WhyNoSaveDesigned(),
          tr("Keeps the last designed line as a voice you can clone from "
             "again, with the line as its words."));
  const QString no_take = gen.WhyNoAdd();
  const bool has_take = no_take.isEmpty();
  take_screen_.SetFile(has_take ? gen.last_take() : QString(),
                       gen.last_voice_name());
  const bool is_playing = preview_.IsPlaying(gen.last_take());
  play_take_.setChecked(is_playing);
  play_take_.setText(is_playing ? tr("Stop") : tr("Play"));
  play_take_.setIcon(style()->standardIcon(
      is_playing ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
  Explain(&play_take_, no_take);
  Explain(&add_, no_take,
          tr("Lays the line on a sound track at the playhead and picks it."));
}

}  // namespace snapper
