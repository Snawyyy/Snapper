// CompPanel: the effect's control rows (CompManager::RowsOf) and their
// values at the playhead.

#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/comp_knobs.h"
#include "edit/comp_manager.h"
#include "ui/angle_dial.h"
#include "ui/comp_panel.h"
#include "ui/fader.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
#include "ui/problem.h"

namespace snapper {

void CompPanel::BuildKnobs(const CompEffect& effect, QVBoxLayout* into) {
  assert(into != nullptr);
  assert(knob_rows_.empty());
  const std::vector<CompRow> rows = CompManager::RowsOf(effect);
  auto* form = MakeOwned<QFormLayout>();
  form->setSpacing(4);
  bool has_keys = false;
  for (const CompRow& row : rows) {
    has_keys = has_keys || row.is_keyable;
    switch (row.kind) {
      case CompRowKind::kFader:
      case CompRowKind::kDial:
        AddNumberRow(row, effect, form);
        break;
      case CompRowKind::kToggle:
        AddToggleRow(row, form);
        break;
      case CompRowKind::kChoice:
        AddChoiceRow(row, form);
        break;
      case CompRowKind::kColor:
        AddColorRow(row, effect, form);
        break;
    }
  }
  into->addLayout(form);
  if (!has_keys) {
    return;
  }
  eased_ = MakeOwned<QCheckBox>(tr("Keys ease"), dynamic_);
  eased_->setToolTip(tr("Off: keys hold until the next one. On: values "
                        "glide between keys."));
  connect(eased_, &QCheckBox::clicked, this, [this](bool is_eased) {
    const CompEffect* picked = Picked();
    const bool is_picked = picked != nullptr;
    if (is_picked) {
      Report(ProblemOf(managers_.comp->SetEased(Shot(), picked->id, is_eased)));
    }
  });
  into->addWidget(eased_);
}

void CompPanel::AddNumberRow(const CompRow& row, const CompEffect& effect,
                             QFormLayout* form) {
  assert(form != nullptr);
  const auto spec = KnobSpecOf(effect, row.knob);
  assert(spec.has_value());
  auto* line = MakeOwned<QHBoxLayout>();
  line->setSpacing(4);
  auto* box = MakeOwned<QDoubleSpinBox>(dynamic_);
  box->setRange(spec->low, spec->high);
  box->setDecimals(row.decimals);
  box->setSingleStep(row.step);
  box->setValue(KnobAt(effect, row.knob, Local()));
  const bool is_dial = row.kind == CompRowKind::kDial;
  if (is_dial) {
    line->addWidget(MakeOwned<AngleDial>(box, &live_, dynamic_));
  } else {
    line->addWidget(MakeOwned<Fader>(box, &live_, spec->high,
                                     spec->neutral, dynamic_),
                    1);
  }
  line->addWidget(box);
  QPushButton* key = nullptr;
  const CompKnob knob = row.knob;
  const bool is_keyable = row.is_keyable;
  if (is_keyable) {
    key = MakeOwned<QPushButton>(tr("Key"), dynamic_);
    key->setCheckable(true);
    key->setFocusPolicy(Qt::NoFocus);
    key->setToolTip(tr("Key this at the playhead, or drop the key there. "
                       "With two keys or more, changes key it."));
    line->addWidget(key);
    connect(key, &QPushButton::clicked, this, [this, knob] {
      const CompEffect* picked = Picked();
      const bool is_picked = picked != nullptr;
      if (is_picked) {
        Report(ProblemOf(
            managers_.comp->ToggleKey(Shot(), picked->id, knob, Local())));
      }
    });
  }
  const QString label = QStringLiteral("%1 %2").arg(
      CompManager::EffectName(effect), row.label.toLower());
  MakeLive(box, &live_, label, this, [this, knob, box] {
    const CompEffect* picked = Picked();
    const bool is_picked = picked != nullptr;
    if (is_picked) {
      Report(ProblemOf(managers_.comp->SetKnob(Shot(), picked->id, knob,
                                               Local(), box->value())));
    }
  });
  form->addRow(row.label, line);
  knob_rows_.push_back(KnobRow{knob, box, key, nullptr, nullptr});
}

void CompPanel::AddToggleRow(const CompRow& row, QFormLayout* form) {
  assert(form != nullptr);
  assert(row.kind == CompRowKind::kToggle);
  auto* toggle = MakeOwned<QCheckBox>(row.label, dynamic_);
  toggle->setToolTip(tr("Holds for the whole shot."));
  const CompKnob knob = row.knob;
  connect(toggle, &QCheckBox::clicked, this, [this, knob](bool is_on) {
    const CompEffect* picked = Picked();
    const bool is_picked = picked != nullptr;
    if (is_picked) {
      Report(ProblemOf(managers_.comp->SetKnob(Shot(), picked->id, knob,
                                               Local(), is_on ? 1.0 : 0.0)));
    }
  });
  form->addRow(toggle);
  knob_rows_.push_back(KnobRow{knob, nullptr, nullptr, toggle, nullptr});
}

void CompPanel::AddChoiceRow(const CompRow& row, QFormLayout* form) {
  assert(form != nullptr);
  assert(row.kind == CompRowKind::kChoice);
  auto* choice = MakeOwned<QComboBox>(dynamic_);
  choice->addItems(row.choices);
  choice->setToolTip(tr("Holds for the whole shot."));
  const CompKnob knob = row.knob;
  connect(choice, &QComboBox::activated, this, [this, knob](int index) {
    const CompEffect* picked = Picked();
    const bool is_choice = picked != nullptr && index >= 0;
    if (is_choice) {
      Report(ProblemOf(managers_.comp->SetKnob(Shot(), picked->id, knob,
                                               Local(), index + 1.0)));
    }
  });
  form->addRow(row.label, choice);
  knob_rows_.push_back(KnobRow{knob, nullptr, nullptr, nullptr, choice});
}

void CompPanel::AddColorRow(const CompRow& row, const CompEffect& effect,
                            QFormLayout* form) {
  assert(form != nullptr);
  assert(row.kind == CompRowKind::kColor);
  const int index = static_cast<int>(row.knob);
  QPushButton* swatch = Swatch(
      CompManager::KnobColor(effect, row.knob, Local()),
      tr("Click, then pick a color in the floating palette."), dynamic_);
  swatch->setCheckable(true);
  swatch->setChecked(aim_ == Aim::kKnobColor && aim_index_ == index);
  connect(swatch, &QPushButton::clicked, this,
          [this, index] { AimAt(Aim::kKnobColor, index); });
  auto* line = MakeOwned<QHBoxLayout>();
  line->addWidget(swatch);
  line->addStretch();
  form->addRow(row.label, line);
}

void CompPanel::ShowValues(const CompEffect& effect) {
  const Frame local = Local();
  const ShotId shot = Shot();
  assert(knob_rows_.size() <= static_cast<size_t>(kCompKnobCount));
  assert(local.index() >= 0);
  for (const KnobRow& row : knob_rows_) {
    ShowRow(row, effect, local);
  }
  const bool has_eased = eased_ != nullptr;
  if (has_eased) {
    eased_->setChecked(CompManager::IsEased(effect));
  }
  const bool can_show_light = light_box_ != nullptr && !light_box_->hasFocus();
  if (can_show_light) {
    ShowNumber(light_box_, managers_.comp->ShotLight(shot, local));
  }
  const bool has_light_key = light_key_ != nullptr;
  if (has_light_key) {
    light_key_->setChecked(managers_.comp->HasShotLightKeyAt(shot, local));
  }
}

void CompPanel::ShowRow(const KnobRow& row, const CompEffect& effect,
                        Frame local) {
  assert(local.index() >= 0);
  assert(static_cast<int>(row.knob) < kCompKnobCount);
  const double value = KnobAt(effect, row.knob, local);
  const bool is_number = row.box != nullptr;
  const bool can_show = is_number && !row.box->hasFocus();
  if (can_show) {
    ShowNumber(row.box, value);
  }
  const bool has_key = row.key != nullptr;
  if (has_key) {
    row.key->setChecked(
        managers_.comp->HasKeyAt(Shot(), effect.id, row.knob, local));
  }
  const bool is_toggle = row.toggle != nullptr;
  if (is_toggle) {
    const QSignalBlocker quiet(row.toggle);
    row.toggle->setChecked(value >= 0.5);
  }
  const bool is_choice = row.choice != nullptr;
  if (is_choice) {
    const QSignalBlocker quiet(row.choice);
    row.choice->setCurrentIndex(
        std::clamp(static_cast<int>(std::lround(value)) - 1, 0,
                   std::max(0, row.choice->count() - 1)));
  }
}

}  // namespace snapper
