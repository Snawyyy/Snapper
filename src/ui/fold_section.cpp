#include "ui/fold_section.h"

#include <QSettings>

#include <cassert>

#include "ui/motion.h"

namespace snapper {
namespace {

QString KeyOf(const QString& key) {
  assert(!key.isEmpty());
  assert(!key.contains('/'));
  return QStringLiteral("folds/") + key;
}

}  // namespace

FoldSection::FoldSection(const QString& title, MotionManager* motion,
                         const QString& settings_path, const QString& key,
                         QWidget* parent, bool is_open_at_first)
    : QWidget(parent), motion_(motion), settings_path_(settings_path),
      key_(key), title_(title), layout_(this), form_(&body_),
      is_open_(is_open_at_first) {
  assert(motion != nullptr);
  assert(!key.isEmpty());
  layout_.setContentsMargins(0, 0, 0, 0);
  header_.setObjectName("fold_header");
  header_.setAutoRaise(true);
  header_.setToolButtonStyle(Qt::ToolButtonTextOnly);
  header_.setToolTip(tr("Show or hide this section."));
  layout_.addWidget(&header_, 0, Qt::AlignLeft);
  layout_.addWidget(&body_);
  const bool is_saved = !settings_path_.isEmpty();
  if (is_saved) {
    const QSettings settings(settings_path_, QSettings::IniFormat);
    is_open_ = settings.value(KeyOf(key_), is_open_).toBool();
  }
  body_.setVisible(is_open_);
  header_.setText((is_open_ ? "v " : "> ") + title_);
  connect(&header_, &QToolButton::clicked, this,
          [this] { SetOpen(!is_open_); });
}

void FoldSection::Remember() const {
  assert(!key_.isEmpty());
  const bool is_saved = !settings_path_.isEmpty();
  assert(is_saved || settings_path_.isEmpty());
  if (is_saved) {
    QSettings settings(settings_path_, QSettings::IniFormat);
    settings.setValue(KeyOf(key_), is_open_);
  }
}

void FoldSection::SetOpen(bool is_open) {
  assert(motion_ != nullptr);
  assert(!title_.isEmpty());
  const bool is_same = is_open == is_open_;
  if (is_same) {
    return;
  }
  is_open_ = is_open;
  header_.setText((is_open_ ? "v " : "> ") + title_);
  Remember();
  const int full = body_.sizeHint().height();
  body_.setVisible(true);
  Slide(is_open_ ? 0 : full, is_open_ ? full : 0);
}

void FoldSection::Slide(int from, int to) {
  assert(from >= 0 && to >= 0);
  assert(motion_ != nullptr);
  const bool is_running = !animation_.isNull();
  if (is_running) {
    animation_->stop();
  }
  animation_ = motion_->Animate(
      this, MotionKind::kMove, from, to, [this](const QVariant& value) {
        body_.setMaximumHeight(value.toInt());
      });
  const bool is_instant = animation_.isNull();
  if (is_instant) {
    Finish();
    return;
  }
  connect(animation_, &QVariantAnimation::finished, this,
          [this] { Finish(); });
}

void FoldSection::Finish() {
  assert(motion_ != nullptr);
  assert(!key_.isEmpty());
  body_.setMaximumHeight(QWIDGETSIZE_MAX);
  body_.setVisible(is_open_);
}

}  // namespace snapper
