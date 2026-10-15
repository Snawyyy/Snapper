#include "ui/fold_section.h"

#include <QSettings>

#include <cassert>

#include "ui/motion.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The margin inside the folded frame, past the indent on the left.
constexpr int kBodyMargin = 4;

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
  // The plate sits right on the frame it folds.
  layout_.setSpacing(0);
  header_.setObjectName("fold_header");
  header_.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  header_.setText(title_);
  QFont bold = header_.font();
  bold.setBold(true);
  header_.setFont(bold);
  header_.setToolTip(tr("Show or hide this section."));
  // A section's bar: full width, raised, with the red strip, like a
  // group box's title (the style draws it).
  header_.setProperty("titlePlate", true);
  header_.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  body_.setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  theme::SetLevel(&body_, theme::kWell);
  form_.setContentsMargins(theme::kIndent, kBodyMargin, kBodyMargin,
                           kBodyMargin);
  layout_.addWidget(&header_);
  layout_.addWidget(&body_);
  const bool is_saved = !settings_path_.isEmpty();
  if (is_saved) {
    const QSettings settings(settings_path_, QSettings::IniFormat);
    is_open_ = settings.value(KeyOf(key_), is_open_).toBool();
  }
  body_.setVisible(is_open_);
  ShowHeader();
  connect(&header_, &QToolButton::clicked, this,
          [this] { SetOpen(!is_open_); });
}

void FoldSection::SetTitle(const QString& title) {
  assert(!title.isEmpty());
  assert(header_.toolButtonStyle() == Qt::ToolButtonTextBesideIcon);
  title_ = title;
  header_.setText(title_);
}

void FoldSection::ShowHeader() {
  assert(!title_.isEmpty());
  assert(header_.toolButtonStyle() == Qt::ToolButtonTextBesideIcon);
  // The style draws the arrow as a square, solid triangle.
  header_.setArrowType(is_open_ ? Qt::DownArrow : Qt::RightArrow);
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
  ShowHeader();
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
