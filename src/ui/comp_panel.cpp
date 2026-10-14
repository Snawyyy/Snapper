#include "ui/comp_panel.h"

#include <QGridLayout>
#include <QSignalBlocker>

#include <array>
#include <cassert>

#include "anim/reel_timeline.h"
#include "edit/comp_manager.h"
#include "edit/history_manager.h"
#include "edit/palette_manager.h"
#include "edit/playback_manager.h"
#include "edit/selection_manager.h"
#include "ui/form_helpers.h"
#include "ui/panel_kit.h"
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

CompPanel::CompPanel(const Managers& managers, CompEffectId effect,
                     QWidget* parent)
    : QWidget(parent),
      managers_(managers),
      live_(managers.history),
      layout_(this),
      effect_(effect),
      pick_colors_(tr("Pick")),
      clear_colors_(tr("Clear")) {
  assert(managers_.IsComplete());
  assert(effect_.IsValid());
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
  layout_.setContentsMargins(0, 0, 0, 0);
  layout_.setSpacing(6);
  note_.setWordWrap(true);
  note_.setStyleSheet(QStringLiteral("QLabel { color: #f2c230; }"));
  note_.hide();
  layout_.addWidget(&note_);
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
  auto* form = MakeOwned<QFormLayout>();
  form->setSpacing(4);
  form->addRow(tr("Blend"), &blend_);
  form->addRow(tr("Colors"), &colors_row_);
  layout_.addLayout(form);
  connect(&blend_, &QComboBox::activated, this, [this](int index) {
    const CompEffect* effect = Effect();
    if (effect == nullptr) {
      return;
    }
    const auto mode = static_cast<BlendMode>(blend_.itemData(index).toInt());
    Report(ProblemOf(managers_.comp->SetBlend(Shot(), effect->id, mode)));
  });
  pick_colors_.setCheckable(true);
  pick_colors_.setFocusPolicy(Qt::NoFocus);
  pick_colors_.setToolTip(
      tr("Work only on some colors: click, then pick them in the floating "
         "palette (Shift adds, Ctrl flips), or press I over the stage."));
  clear_colors_.setFocusPolicy(Qt::NoFocus);
  colors_row_.addWidget(&pick_colors_);
  colors_row_.addWidget(&clear_colors_);
  colors_row_.addStretch();
  connect(&pick_colors_, &QPushButton::clicked, this,
          [this] { AimAt(Aim::kTarget, 0); });
  connect(&clear_colors_, &QPushButton::clicked, this, [this] {
    const CompEffect* effect = Effect();
    if (effect != nullptr) {
      Report(ProblemOf(managers_.comp->ClearTargetColors(Shot(), effect->id)));
    }
  });
  dynamic_ = new QWidget(this);
  layout_.addWidget(dynamic_);
}

ShotId CompPanel::Shot() const { return managers_.selection->shot(); }

Frame CompPanel::Local() const {
  const Frame playhead = managers_.playback->FrameOn(Timeline::kReel);
  const ReelShot at = ShotAtReel(managers_.history->current(), playhead);
  return at.IsValid() ? at.local : Frame(0);
}

const CompEffect* CompPanel::Effect() const {
  return managers_.comp->Find(Shot(), effect_);
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
  // The rows (a switch may show or hide others, as the Line smoother's
  // do), and the colors their swatches show.
  signature += QStringLiteral("p") + effect->plugin;
  for (const CompRow& row : CompManager::RowsOf(*effect)) {
    signature += QStringLiteral("r%1").arg(static_cast<int>(row.knob));
    const bool is_color = row.kind == CompRowKind::kColor;
    if (is_color) {
      signature += QStringLiteral("c") +
                   ColorKey(CompManager::KnobColor(*effect, row.knob, Local()));
    }
  }
  return signature;
}

void CompPanel::Refresh() {
  assert(managers_.comp != nullptr);
  assert(effect_.IsValid());
  const CompEffect* effect = Effect();
  const bool has_effect = effect != nullptr;
  if (!has_effect) {
    return;
  }
  const QString why_not_run = managers_.comp->WhyNotRun(*effect);
  note_.setText(why_not_run);
  note_.setVisible(!why_not_run.isEmpty());
  {
    const QSignalBlocker quiet(&blend_);
    blend_.setCurrentIndex(blend_.findData(static_cast<int>(effect->blend)));
  }
  pick_colors_.setChecked(aim_ == Aim::kTarget);
  const bool has_colors = !effect->target.colors.empty();
  Explain(&clear_colors_, has_colors ? QString() : tr("No colors are picked."),
          tr("Work on every color again."));
  const QString signature = Signature(effect);
  const bool is_new = signature != signature_;
  if (is_new) {
    signature_ = signature;
    Rebuild(effect);
  }
  ShowValues(*effect);
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
        const CompEffect* picked = Effect();
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
        const CompEffect* picked = Effect();
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
      const CompEffect* picked = Effect();
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
      const CompEffect* picked = Effect();
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
              const CompEffect* picked = Effect();
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
    const CompEffect* picked = Effect();
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
      const CompEffect* picked = Effect();
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
    const CompEffect* picked = Effect();
    const QColor from = managers_.palette != nullptr
                            ? managers_.palette->current()
                            : QColor(Qt::black);
    if (picked != nullptr) {
      Report(ProblemOf(managers_.comp->AddSwap(Shot(), picked->id, from)));
    }
  });
  into->addWidget(add);
}

void CompPanel::AimAt(Aim aim, int index) {
  assert(index >= 0);
  // A second click on the aimed control stops aiming.
  const bool is_same = aim_ == aim && aim_index_ == index;
  aim_ = is_same ? Aim::kNone : aim;
  aim_index_ = index;
  const bool is_aimed = aim_ != Aim::kNone;
  if (is_aimed) {
    // Only the picked effect's panel takes palette colors.
    managers_.comp->Pick(Shot(), effect_);
  }
  const bool can_show_palette = is_aimed && managers_.palette != nullptr;
  if (can_show_palette) {
    managers_.palette->SetUse(PaletteUse::kFree);
    managers_.palette->SetShown(true);
  }
  signature_.clear();
  Refresh();
}

void CompPanel::TakeColor(const QColor& color,
                          Qt::KeyboardModifiers modifiers) {
  const bool is_aimed = aim_ != Aim::kNone && isVisible() &&
                        color.isValid() &&
                        managers_.comp->PickedIn(Shot()) == effect_;
  const CompEffect* effect = Effect();
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
    case Aim::kKnobColor:
      Report(ProblemOf(managers_.comp->SetKnobColor(
          shot, effect->id, static_cast<CompKnob>(aim_index_), color)));
      break;
    case Aim::kNone:
      break;
  }
}

}  // namespace snapper
