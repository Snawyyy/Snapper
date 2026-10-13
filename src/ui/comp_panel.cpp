#include "ui/comp_panel.h"

#include <QFormLayout>
#include <QGridLayout>
#include <QSignalBlocker>

#include <cassert>
#include <cmath>

#include "anim/comp_knobs.h"
#include "anim/reel_timeline.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/angle_dial.h"
#include "ui/fader.h"
#include "ui/form_helpers.h"
#include "ui/problem.h"

namespace snapper {
namespace {

constexpr int kSwatch = 22;

// The blend modes in their groups; a separator between groups.
constexpr std::array<std::array<int, 2>, 6> kBlendGroups = {{
    {0, 0}, {1, 4}, {5, 8}, {9, 11}, {12, 14}, {15, 18}}};

QString ColorKey(const QColor& color) {
  return QString::number(color.rgba(), 16);
}

}  // namespace

CompPanel::CompPanel(const Managers& managers)
    : managers_(managers),
      live_(managers.history),
      layout_(this),
      applies_title_(tr("Applies to")),
      tiles_{QPushButton(tr("Everything")), QPushButton(tr("A layer")),
             QPushButton(tr("Pieces"))},
      pick_colors_(tr("Pick colors")),
      clear_colors_(tr("Clear colors")) {
  assert(managers_.IsComplete());
  assert(managers_.comp != nullptr);
  BuildFixed();
  const auto refresh = [this] { Refresh(); };
  connect(managers_.history, &HistoryManager::Changed, this, refresh);
  connect(managers_.comp, &CompManager::Changed, this, refresh);
  connect(managers_.selection, &SelectionManager::Changed, this, refresh);
  connect(managers_.playback, &PlaybackManager::FrameChanged, this, refresh);
  if (managers_.palette != nullptr) {
    connect(managers_.palette, &PaletteManager::Picked, this,
            &CompPanel::TakeColor);
  }
  Refresh();
}

void CompPanel::BuildFixed() {
  assert(layout_.count() == 0);
  layout_.setContentsMargins(8, 8, 8, 8);
  layout_.setSpacing(6);
  QFont bold = title_.font();
  bold.setBold(true);
  bold.setPointSize(bold.pointSize() + 2);
  title_.setFont(bold);
  layout_.addWidget(&title_);
  help_.setWordWrap(true);
  layout_.addWidget(&help_);
  for (const auto& group : kBlendGroups) {
    const bool is_after_first = blend_.count() > 0;
    if (is_after_first) {
      blend_.insertSeparator(blend_.count());
    }
    for (int mode = group[0]; mode <= group[1]; ++mode) {
      blend_.addItem(CompManager::BlendName(static_cast<BlendMode>(mode)),
                     mode);
    }
  }
  blend_.setToolTip(tr("How the effect mixes with what is below. "
                       "Shift+= and Shift+- step through the modes."));
  layout_.addWidget(&blend_);
  connect(&blend_, &QComboBox::activated, this, [this](int index) {
    const CompEffect* effect = Picked();
    if (effect == nullptr) {
      return;
    }
    const auto mode = static_cast<BlendMode>(blend_.itemData(index).toInt());
    Report(ProblemOf(managers_.comp->SetBlend(Shot(), effect->id, mode)));
  });
  layout_.addWidget(&applies_title_);
  tiles_row_.setSpacing(2);
  for (QPushButton& tile : tiles_) {
    tile.setCheckable(true);
    tile.setFocusPolicy(Qt::NoFocus);
    tiles_row_.addWidget(&tile);
  }
  layout_.addLayout(&tiles_row_);
  connect(&tiles_[0], &QPushButton::clicked, this, [this] {
    const CompEffect* effect = Picked();
    if (effect != nullptr) {
      Report(ProblemOf(managers_.comp->TargetEverything(Shot(), effect->id)));
    }
  });
  connect(&tiles_[1], &QPushButton::clicked, this, [this] {
    const CompEffect* effect = Picked();
    if (effect != nullptr) {
      Report(ProblemOf(managers_.comp->TargetLayer(
          Shot(), effect->id, managers_.selection->layer())));
    }
  });
  connect(&tiles_[2], &QPushButton::clicked, this, [this] {
    const CompEffect* effect = Picked();
    if (effect != nullptr) {
      Report(ProblemOf(managers_.comp->TargetPieces(
          Shot(), effect->id, managers_.selection->layer(),
          managers_.selection->pieces())));
    }
  });
  target_text_.setWordWrap(true);
  layout_.addWidget(&target_text_);
  pick_colors_.setCheckable(true);
  pick_colors_.setFocusPolicy(Qt::NoFocus);
  pick_colors_.setToolTip(
      tr("Click, then pick colors in the floating palette (Shift adds, "
         "Ctrl flips), or press I over the stage."));
  clear_colors_.setFocusPolicy(Qt::NoFocus);
  colors_row_.addWidget(&pick_colors_);
  colors_row_.addWidget(&clear_colors_);
  colors_row_.addStretch();
  layout_.addLayout(&colors_row_);
  connect(&pick_colors_, &QPushButton::clicked, this,
          [this] { AimAt(Aim::kTarget, 0); });
  connect(&clear_colors_, &QPushButton::clicked, this, [this] {
    const CompEffect* effect = Picked();
    if (effect != nullptr) {
      Report(ProblemOf(managers_.comp->ClearTargetColors(Shot(), effect->id)));
    }
  });
  dynamic_ = new QWidget(this);
  layout_.addWidget(dynamic_);
  layout_.addStretch();
}

ShotId CompPanel::Shot() const { return managers_.selection->shot(); }

Frame CompPanel::Local() const {
  const Frame playhead = managers_.playback->FrameOn(Timeline::kReel);
  const ReelShot at = ShotAtReel(managers_.history->current(), playhead);
  return at.IsValid() ? at.local : Frame(0);
}

const CompEffect* CompPanel::Picked() const {
  return managers_.comp->Find(Shot(), managers_.comp->PickedIn(Shot()));
}

QString CompPanel::Signature(const CompEffect* effect) const {
  if (effect == nullptr) {
    return QStringLiteral("none");
  }
  QString signature = QStringLiteral("%1/%2/%3/%4/%5/%6/%7")
                          .arg(effect->id.value())
                          .arg(static_cast<int>(effect->kind))
                          .arg(static_cast<int>(effect->shape))
                          .arg(effect->style)
                          .arg(effect->uses_shot_light)
                          .arg(ColorKey(effect->color))
                          .arg(Shot().value());
  for (const GradientStop& stop : effect->stops) {
    signature += QStringLiteral("s") + ColorKey(stop.color);
  }
  for (const ColorSwap& swap : effect->swaps) {
    signature += QStringLiteral("w") + ColorKey(swap.from) + ColorKey(swap.to);
  }
  for (const QColor& color : effect->target.colors) {
    signature += QStringLiteral("t") + ColorKey(color);
  }
  return signature;
}

void CompPanel::Refresh() {
  assert(managers_.comp != nullptr);
  const CompEffect* effect = Picked();
  const bool has_effect = effect != nullptr;
  title_.setText(has_effect ? CompManager::KindName(effect->kind)
                            : tr("No effect picked"));
  help_.setText(has_effect ? CompManager::KindHelp(effect->kind)
                           : tr("Add an effect with + or A, or pick one in "
                                "the stack."));
  const QString why_none = has_effect ? QString() : tr("Pick an effect first.");
  Explain(&blend_, why_none, blend_.toolTip());
  if (has_effect) {
    const QSignalBlocker quiet(&blend_);
    blend_.setCurrentIndex(
        blend_.findData(static_cast<int>(effect->blend)));
  }
  const ShotId shot = Shot();
  const TargetTile tile = has_effect ? CompManager::TileOf(*effect)
                                     : TargetTile::kEverything;
  const QString why_layer =
      managers_.comp->WhyNoTargetLayer(shot, managers_.selection->layer());
  const QString why_pieces = managers_.comp->WhyNoTargetPieces(
      shot, managers_.selection->layer(), managers_.selection->pieces());
  Explain(&tiles_[0], why_none,
          tr("Everything below: the whole frame, in screen space."));
  Explain(&tiles_[1], has_effect ? why_layer : why_none,
          tr("A layer: the picked layer only; the effect follows it. "
             "Click a doll on the stage to pick it."));
  Explain(&tiles_[2], has_effect ? why_pieces : why_none,
          tr("Picked pieces: those pieces of the doll only. Click a piece "
             "on the stage, Shift adds."));
  for (size_t i = 0; i < tiles_.size(); ++i) {
    tiles_[i].setChecked(has_effect && static_cast<int>(tile) ==
                                           static_cast<int>(i));
  }
  applies_title_.setEnabled(has_effect);
  target_text_.setText(has_effect ? managers_.comp->TargetText(shot, *effect)
                                  : QString());
  Explain(&pick_colors_, why_none, pick_colors_.toolTip());
  pick_colors_.setChecked(aim_ == Aim::kTarget);
  const bool has_colors = has_effect && !effect->target.colors.empty();
  Explain(&clear_colors_,
          !has_effect   ? why_none
          : !has_colors ? tr("No colors are picked.")
                        : QString(),
          tr("Work on every color again."));
  const QString signature = Signature(effect);
  const bool is_new = signature != signature_;
  if (is_new) {
    signature_ = signature;
    Rebuild(effect);
  }
  if (has_effect) {
    ShowValues(*effect);
  }
}

QPushButton* CompPanel::Swatch(const QColor& color, const QString& tip,
                               QWidget* parent) {
  assert(parent != nullptr);
  auto* swatch = new QPushButton(parent);
  swatch->setFixedSize(kSwatch, kSwatch);
  swatch->setFocusPolicy(Qt::NoFocus);
  swatch->setToolTip(QStringLiteral("%1 (%2)").arg(
      tip, color.name(QColor::HexArgb)));
  // Square, the color itself; a Teto red edge while it waits for a
  // palette pick (see AimAt).
  const auto paint = [swatch, color](bool is_aimed) {
    swatch->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: %2; }")
            .arg(color.name(),
                 is_aimed ? QStringLiteral("2px solid #d8304f")
                          : QStringLiteral("1px solid #101012")));
  };
  paint(false);
  connect(swatch, &QPushButton::toggled, swatch, paint);
  return swatch;
}

void CompPanel::Rebuild(const CompEffect* effect) {
  assert(dynamic_ != nullptr);
  knob_rows_.clear();
  light_box_ = nullptr;
  light_key_ = nullptr;
  eased_ = nullptr;
  const int at = layout_.indexOf(dynamic_);
  dynamic_->hide();
  dynamic_->deleteLater();
  dynamic_ = new QWidget(this);
  layout_.insertWidget(at, dynamic_);
  auto* into = new QVBoxLayout(dynamic_);
  into->setContentsMargins(0, 0, 0, 0);
  into->setSpacing(6);
  if (effect == nullptr) {
    return;
  }
  const bool has_colors = !effect->target.colors.empty();
  if (has_colors) {
    auto* row = new QHBoxLayout();
    row->setSpacing(2);
    for (const QColor& color : effect->target.colors) {
      QPushButton* swatch =
          Swatch(color, tr("A picked target color. Click to drop it."),
                 dynamic_);
      connect(swatch, &QPushButton::clicked, this, [this, color] {
        const CompEffect* picked = Picked();
        if (picked != nullptr) {
          Report(ProblemOf(managers_.comp->PickTargetColor(
              Shot(), picked->id, color, Qt::ControlModifier)));
        }
      });
      row->addWidget(swatch);
    }
    row->addStretch();
    into->addLayout(row);
  }
  BuildLook(*effect, into);
  BuildGradient(*effect, into);
  BuildSwaps(*effect, into);
  BuildKnobs(*effect, into);
  BuildLight(into);
  dynamic_->show();
}

void CompPanel::BuildLook(const CompEffect& effect, QVBoxLayout* into) {
  assert(into != nullptr);
  const bool uses_color = CompManager::UsesColor(effect);
  if (uses_color) {
    auto* row = new QHBoxLayout();
    auto* label = new QLabel(tr("Color"), dynamic_);
    QPushButton* color = Swatch(
        effect.color, tr("Click, then pick a color in the floating palette."),
        dynamic_);
    color->setCheckable(true);
    color->setChecked(aim_ == Aim::kColor);
    connect(color, &QPushButton::clicked, this,
            [this] { AimAt(Aim::kColor, 0); });
    row->addWidget(label);
    row->addWidget(color);
    row->addStretch();
    into->addLayout(row);
  }
  const std::vector<QString> styles = CompManager::StyleNames(effect.kind);
  const bool has_styles = !styles.empty();
  if (has_styles) {
    auto* row = new QHBoxLayout();
    row->setSpacing(2);
    for (size_t i = 0; i < styles.size(); ++i) {
      auto* tile = new QPushButton(styles[i], dynamic_);
      tile->setCheckable(true);
      tile->setFocusPolicy(Qt::NoFocus);
      tile->setChecked(effect.style == static_cast<int>(i));
      const int style = static_cast<int>(i);
      connect(tile, &QPushButton::clicked, this, [this, style] {
        const CompEffect* picked = Picked();
        if (picked != nullptr) {
          Report(ProblemOf(
              managers_.comp->SetStyle(Shot(), picked->id, style)));
        }
      });
      row->addWidget(tile);
    }
    into->addLayout(row);
  }
  const bool can_share = CompManager::CanUseShotLight(effect);
  if (can_share) {
    auto* shared = new QCheckBox(tr("Aim by the shot light"), dynamic_);
    shared->setChecked(effect.uses_shot_light);
    shared->setToolTip(tr("Follow the shot's Light, set once below, so "
                          "every doll is lit from the same side."));
    connect(shared, &QCheckBox::clicked, this, [this](bool is_used) {
      const CompEffect* picked = Picked();
      if (picked != nullptr) {
        Report(ProblemOf(
            managers_.comp->SetUsesShotLight(Shot(), picked->id, is_used)));
      }
    });
    into->addWidget(shared);
  }
}

void CompPanel::BuildGradient(const CompEffect& effect, QVBoxLayout* into) {
  assert(into != nullptr);
  const bool is_gradient = effect.kind == CompKind::kGradient;
  if (!is_gradient) {
    return;
  }
  auto* shapes = new QGridLayout();
  shapes->setSpacing(2);
  for (int i = 0; i < kGradientShapeCount; ++i) {
    const auto shape = static_cast<GradientShape>(i);
    auto* tile = new QPushButton(CompManager::ShapeName(shape), dynamic_);
    tile->setCheckable(true);
    tile->setFocusPolicy(Qt::NoFocus);
    tile->setChecked(effect.shape == shape);
    connect(tile, &QPushButton::clicked, this, [this, shape] {
      const CompEffect* picked = Picked();
      if (picked != nullptr) {
        Report(ProblemOf(managers_.comp->SetShape(Shot(), picked->id, shape)));
      }
    });
    shapes->addWidget(tile, i / 3, i % 3);
  }
  into->addLayout(shapes);
  auto* stops = new QHBoxLayout();
  stops->setSpacing(2);
  const bool is_corners = effect.shape == GradientShape::kFourCorner;
  stops->addWidget(new QLabel(is_corners ? tr("Corners") : tr("Colors"),
                              dynamic_));
  const QString why_remove = managers_.comp->WhyNoRemoveStop(effect);
  for (size_t i = 0; i < effect.stops.size(); ++i) {
    const bool is_corner_extra = is_corners && i >= 4;
    if (is_corner_extra) {
      break;
    }
    const int stop = static_cast<int>(i);
    QPushButton* swatch = Swatch(
        effect.stops[i].color,
        why_remove.isEmpty()
            ? tr("Click, then pick its color in the palette. Right click "
                 "removes it.")
            : tr("Click, then pick its color in the palette."),
        dynamic_);
    swatch->setCheckable(true);
    swatch->setChecked(aim_ == Aim::kStop && aim_index_ == stop);
    swatch->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(swatch, &QPushButton::clicked, this,
            [this, stop] { AimAt(Aim::kStop, stop); });
    connect(swatch, &QWidget::customContextMenuRequested, this,
            [this, stop] {
              const CompEffect* picked = Picked();
              if (picked != nullptr) {
                Report(ProblemOf(
                    managers_.comp->RemoveStop(Shot(), picked->id, stop)));
              }
            });
    stops->addWidget(swatch);
  }
  auto* add = new QPushButton(tr("+"), dynamic_);
  add->setFixedWidth(kSwatch + 6);
  add->setFocusPolicy(Qt::NoFocus);
  Explain(add, managers_.comp->WhyNoAddStop(effect),
          tr("Add a color stop to the ramp."));
  connect(add, &QPushButton::clicked, this, [this] {
    const CompEffect* picked = Picked();
    if (picked != nullptr) {
      Report(ProblemOf(managers_.comp->AddStop(Shot(), picked->id)));
    }
  });
  stops->addWidget(add);
  stops->addStretch();
  into->addLayout(stops);
}

void CompPanel::BuildSwaps(const CompEffect& effect, QVBoxLayout* into) {
  assert(into != nullptr);
  const bool is_swap = effect.kind == CompKind::kColorSwap;
  if (!is_swap) {
    return;
  }
  for (size_t i = 0; i < effect.swaps.size(); ++i) {
    const int swap = static_cast<int>(i);
    auto* row = new QHBoxLayout();
    row->setSpacing(4);
    QPushButton* from =
        Swatch(effect.swaps[i].from,
               tr("The color to swap. Click, then pick it in the palette."),
               dynamic_);
    QPushButton* to =
        Swatch(effect.swaps[i].to,
               tr("What it becomes. Click, then pick it in the palette."),
               dynamic_);
    from->setCheckable(true);
    to->setCheckable(true);
    from->setChecked(aim_ == Aim::kSwapFrom && aim_index_ == swap);
    to->setChecked(aim_ == Aim::kSwapTo && aim_index_ == swap);
    connect(from, &QPushButton::clicked, this,
            [this, swap] { AimAt(Aim::kSwapFrom, swap); });
    connect(to, &QPushButton::clicked, this,
            [this, swap] { AimAt(Aim::kSwapTo, swap); });
    auto* remove = new QPushButton(tr("x"), dynamic_);
    remove->setFixedWidth(kSwatch + 2);
    remove->setFocusPolicy(Qt::NoFocus);
    remove->setToolTip(tr("Remove this pair."));
    connect(remove, &QPushButton::clicked, this, [this, swap] {
      const CompEffect* picked = Picked();
      if (picked != nullptr) {
        Report(ProblemOf(managers_.comp->RemoveSwap(Shot(), picked->id, swap)));
      }
    });
    row->addWidget(from);
    row->addWidget(new QLabel(tr("to"), dynamic_));
    row->addWidget(to);
    row->addWidget(remove);
    row->addStretch();
    into->addLayout(row);
  }
  auto* add = new QPushButton(tr("+ Color pair"), dynamic_);
  add->setFocusPolicy(Qt::NoFocus);
  Explain(add, managers_.comp->WhyNoAddSwap(effect),
          tr("Add a pair, starting from the palette's current color."));
  connect(add, &QPushButton::clicked, this, [this] {
    const CompEffect* picked = Picked();
    const QColor from = managers_.palette != nullptr
                            ? managers_.palette->current()
                            : QColor(Qt::black);
    if (picked != nullptr) {
      Report(ProblemOf(managers_.comp->AddSwap(Shot(), picked->id, from)));
    }
  });
  into->addWidget(add);
}

void CompPanel::BuildKnobs(const CompEffect& effect, QVBoxLayout* into) {
  assert(into != nullptr);
  assert(knob_rows_.empty());
  auto* form = new QFormLayout();
  form->setSpacing(4);
  for (int i = 0; i < kCompKnobCount; ++i) {
    const auto knob = static_cast<CompKnob>(i);
    const bool is_shown = CompManager::IsKnobShown(effect, knob);
    if (!is_shown) {
      continue;
    }
    const KnobSpec spec = *KnobSpecOf(effect.kind, knob);
    auto* row = new QHBoxLayout();
    row->setSpacing(4);
    auto* box = new QDoubleSpinBox(dynamic_);
    box->setRange(spec.low, spec.high);
    const bool is_fraction = spec.high <= 1.0;
    box->setDecimals(is_fraction ? 2 : 0);
    box->setSingleStep(is_fraction ? 0.05 : 1.0);
    box->setValue(KnobAt(effect, knob, Local()));
    if (spec.is_angle) {
      row->addWidget(new AngleDial(box, &live_, dynamic_));
    } else {
      row->addWidget(new Fader(box, &live_, spec.high, spec.neutral, dynamic_),
                     1);
    }
    row->addWidget(box);
    auto* key = new QPushButton(tr("Key"), dynamic_);
    key->setCheckable(true);
    key->setFocusPolicy(Qt::NoFocus);
    key->setToolTip(tr("Key this at the playhead, or drop the key there. "
                       "With two keys or more, changes key it."));
    row->addWidget(key);
    const QString label = QStringLiteral("%1 %2").arg(
        CompManager::KindName(effect.kind),
        CompManager::KnobLabel(effect.kind, knob).toLower());
    MakeLive(box, &live_, label, this, [this, knob, box] {
      const CompEffect* picked = Picked();
      if (picked != nullptr) {
        Report(ProblemOf(managers_.comp->SetKnob(Shot(), picked->id, knob,
                                                 Local(), box->value())));
      }
    });
    connect(key, &QPushButton::clicked, this, [this, knob] {
      const CompEffect* picked = Picked();
      if (picked != nullptr) {
        Report(ProblemOf(
            managers_.comp->ToggleKey(Shot(), picked->id, knob, Local())));
      }
    });
    form->addRow(CompManager::KnobLabel(effect.kind, knob), row);
    knob_rows_.push_back(KnobRow{knob, box, key});
  }
  into->addLayout(form);
  eased_ = new QCheckBox(tr("Keys ease"), dynamic_);
  eased_->setToolTip(tr("Off: keys hold until the next one. On: values "
                        "glide between keys."));
  connect(eased_, &QCheckBox::clicked, this, [this](bool is_eased) {
    const CompEffect* picked = Picked();
    if (picked != nullptr) {
      Report(ProblemOf(managers_.comp->SetEased(Shot(), picked->id, is_eased)));
    }
  });
  into->addWidget(eased_);
}

void CompPanel::BuildLight(QVBoxLayout* into) {
  assert(into != nullptr);
  auto* title = new QLabel(tr("Shot light"), dynamic_);
  QFont bold = title->font();
  bold.setBold(true);
  title->setFont(bold);
  into->addWidget(title);
  auto* row = new QHBoxLayout();
  light_box_ = new QDoubleSpinBox(dynamic_);
  light_box_->setRange(0.0, 360.0);
  light_box_->setDecimals(0);
  light_box_->setWrapping(true);
  light_box_->setSuffix(tr(" deg"));
  light_box_->setToolTip(tr("Where the light comes from (0 up, clockwise), "
                            "for every rim light and gradient that follows "
                            "the shot light."));
  row->addWidget(new AngleDial(light_box_, &live_, dynamic_, 36));
  row->addWidget(light_box_);
  light_key_ = new QPushButton(tr("Key"), dynamic_);
  light_key_->setCheckable(true);
  light_key_->setFocusPolicy(Qt::NoFocus);
  light_key_->setToolTip(tr("Key the shot light at the playhead, so a light "
                            "can swing across the shot."));
  row->addWidget(light_key_);
  row->addStretch();
  into->addLayout(row);
  QDoubleSpinBox* box = light_box_;
  MakeLive(light_box_, &live_, tr("Shot light"), this, [this, box] {
    Report(ProblemOf(
        managers_.comp->SetShotLight(Shot(), Local(), box->value())));
  });
  connect(light_key_, &QPushButton::clicked, this, [this] {
    Report(ProblemOf(managers_.comp->ToggleShotLightKey(Shot(), Local())));
  });
}

void CompPanel::ShowValues(const CompEffect& effect) {
  const Frame local = Local();
  const ShotId shot = Shot();
  for (const KnobRow& row : knob_rows_) {
    assert(row.box != nullptr && row.key != nullptr);
    const bool is_typing = row.box->hasFocus();
    if (!is_typing) {
      ShowNumber(row.box, KnobAt(effect, row.knob, local));
    }
    row.key->setChecked(
        managers_.comp->HasKeyAt(shot, effect.id, row.knob, local));
  }
  if (eased_ != nullptr) {
    eased_->setChecked(CompManager::IsEased(effect));
  }
  if (light_box_ != nullptr && !light_box_->hasFocus()) {
    ShowNumber(light_box_, managers_.comp->ShotLight(shot, local));
  }
  if (light_key_ != nullptr) {
    light_key_->setChecked(managers_.comp->HasShotLightKeyAt(shot, local));
  }
}

void CompPanel::AimAt(Aim aim, int index) {
  assert(index >= 0);
  // A second click on the aimed control stops aiming.
  const bool is_same = aim_ == aim && aim_index_ == index;
  aim_ = is_same ? Aim::kNone : aim;
  aim_index_ = index;
  const bool is_aimed = aim_ != Aim::kNone;
  if (is_aimed && managers_.palette != nullptr) {
    managers_.palette->SetShown(true);
  }
  signature_.clear();
  Refresh();
}

void CompPanel::TakeColor(const QColor& color,
                          Qt::KeyboardModifiers modifiers) {
  const bool is_aimed = aim_ != Aim::kNone && isVisible() && color.isValid();
  const CompEffect* effect = Picked();
  if (!is_aimed || effect == nullptr) {
    return;
  }
  const ShotId shot = Shot();
  switch (aim_) {
    case Aim::kColor:
      Report(ProblemOf(managers_.comp->SetColor(shot, effect->id, color)));
      break;
    case Aim::kTarget:
      Report(ProblemOf(managers_.comp->PickTargetColor(shot, effect->id,
                                                       color, modifiers)));
      break;
    case Aim::kStop:
      Report(ProblemOf(
          managers_.comp->SetStopColor(shot, effect->id, aim_index_, color)));
      break;
    case Aim::kSwapFrom:
    case Aim::kSwapTo:
      Report(ProblemOf(managers_.comp->SetSwapColor(
          shot, effect->id, aim_index_, aim_ == Aim::kSwapTo, color)));
      break;
    case Aim::kNone:
      break;
  }
}

}  // namespace snapper
