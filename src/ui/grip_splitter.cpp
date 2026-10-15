#include "ui/grip_splitter.h"

#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QShowEvent>

#include <algorithm>
#include <cassert>
#include <memory>
#include <numeric>

#include "ui/snapper_style.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Saved sizes are shares of the whole, in parts per this many, so a
// window of another size gets the same look.
constexpr int kWhole = 10000;
constexpr int kMaxPanes = 16;

QString SettingKey(const QString& key) {
  assert(!key.isEmpty());
  assert(!key.contains('/'));
  return QStringLiteral("splits/") + key;
}

}  // namespace

GripHandle::GripHandle(Qt::Orientation orientation, QSplitter* parent)
    : QSplitterHandle(orientation, parent) {
  assert(parent != nullptr);
  assert(kGripDwellMs > 0);
  dwell_.setSingleShot(true);
  dwell_.setInterval(kGripDwellMs);
  connect(&dwell_, &QTimer::timeout, this, [this] {
    is_lit_ = true;
    update();
  });
}

void GripHandle::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(rect().isValid());
  QPainter painter(this);
  // The gap between two panels: the window shows through.
  painter.fillRect(rect(), theme::kGap);
  const bool is_active = is_lit_ || is_down_;
  DrawGripLine(&painter, rect(), is_active);
}

void GripHandle::enterEvent(QEnterEvent* event) {
  assert(event != nullptr);
  assert(event->position().x() >= -1.0);
  QSplitterHandle::enterEvent(event);
  dwell_.start();
}

void GripHandle::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(kGripDwellMs > 0);
  QSplitterHandle::leaveEvent(event);
  dwell_.stop();
  is_lit_ = false;
  update();
}

void GripHandle::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(event->button() != Qt::NoButton);
  QSplitterHandle::mousePressEvent(event);
  is_down_ = true;
  update();
}

void GripHandle::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(event->button() != Qt::NoButton);
  QSplitterHandle::mouseReleaseEvent(event);
  is_down_ = false;
  update();
  emit Released();
}

void GripHandle::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(parentWidget() != nullptr);
  QSplitterHandle::mouseDoubleClickEvent(event);
  emit DoubleClicked();
}

GripSplitter::GripSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent) {
  assert(orientation == Qt::Horizontal || orientation == Qt::Vertical);
  assert(handleWidth() > 0);
}

QSplitterHandle* GripSplitter::createHandle() {
  assert(count() >= 0);
  assert(count() < kMaxPanes);
  // The splitter owns its handles.
  auto handle = std::make_unique<GripHandle>(orientation(), this);
  connect(handle.get(), &GripHandle::Released, this,
          [this] { SaveSizes(); });
  connect(handle.get(), &GripHandle::DoubleClicked, this,
          [this] { ResetSizes(); });
  return handle.release();
}

void GripSplitter::SetDefaultShares(const QList<int>& shares) {
  assert(shares.size() <= kMaxPanes);
  assert(shares.size() == count());
  shares_ = shares;
  for (int i = 0; i < shares.size(); ++i) {
    setStretchFactor(i, shares[i]);
  }
}

void GripSplitter::Remember(const QString& key,
                            const QString& settings_path) {
  assert(!key.isEmpty());
  assert(!key.contains('/'));
  key_ = key;
  settings_path_ = settings_path;
}

QList<int> GripSplitter::DefaultSizes(int total) const {
  assert(total >= 0);
  assert(shares_.size() == count());
  const bool is_across = orientation() == Qt::Horizontal;
  QList<int> sizes(count(), 0);
  int fixed = 0;
  int share_sum = 0;
  for (int i = 0; i < count(); ++i) {
    const bool is_fixed = shares_[i] == 0;
    if (is_fixed) {
      const QSize least = widget(i)->minimumSizeHint().expandedTo(
          widget(i)->minimumSize());
      sizes[i] = is_across ? least.width() : least.height();
      fixed += sizes[i];
    }
    share_sum += shares_[i];
  }
  const int rest = std::max(0, total - fixed);
  const bool has_shares = share_sum > 0;
  for (int i = 0; i < count() && has_shares; ++i) {
    const bool is_shared = shares_[i] > 0;
    if (is_shared) {
      sizes[i] = rest * shares_[i] / share_sum;
    }
  }
  return sizes;
}

void GripSplitter::ResetSizes() {
  assert(count() <= kMaxPanes);
  assert(shares_.size() <= kMaxPanes);
  const bool has_defaults = shares_.size() == count() && count() > 0;
  if (!has_defaults) {
    return;
  }
  const QList<int> now = sizes();
  setSizes(DefaultSizes(std::accumulate(now.begin(), now.end(), 0)));
  const bool is_kept = !key_.isEmpty() && !settings_path_.isEmpty();
  if (is_kept) {
    QSettings settings(settings_path_, QSettings::IniFormat);
    settings.remove(SettingKey(key_));
  }
}

void GripSplitter::SaveSizes() const {
  assert(count() <= kMaxPanes);
  assert(kWhole > 0);
  const bool is_kept = !key_.isEmpty() && !settings_path_.isEmpty();
  if (!is_kept) {
    return;
  }
  const QList<int> now = sizes();
  const int total = std::accumulate(now.begin(), now.end(), 0);
  const bool is_laid_out = total > 0;
  if (!is_laid_out) {
    return;
  }
  QList<QVariant> parts;
  for (const int size : now) {
    parts.append(static_cast<int>(static_cast<qint64>(size) * kWhole / total));
  }
  QSettings settings(settings_path_, QSettings::IniFormat);
  settings.setValue(SettingKey(key_), parts);
}

void GripSplitter::RestoreSizes() {
  assert(count() <= kMaxPanes);
  assert(kWhole > 0);
  const bool is_kept = !key_.isEmpty() && !settings_path_.isEmpty();
  if (!is_kept) {
    return;
  }
  const QSettings settings(settings_path_, QSettings::IniFormat);
  const QList<QVariant> parts = settings.value(SettingKey(key_)).toList();
  const bool is_usable = parts.size() == count() && count() > 0;
  if (!is_usable) {
    return;
  }
  const QList<int> now = sizes();
  const int total = std::accumulate(now.begin(), now.end(), 0);
  QList<int> wanted;
  for (const QVariant& part : parts) {
    const qint64 share = part.toInt();
    wanted.append(static_cast<int>(share * total / kWhole));
  }
  setSizes(wanted);
}

void GripSplitter::showEvent(QShowEvent* event) {
  assert(event != nullptr);
  assert(count() <= kMaxPanes);
  QSplitter::showEvent(event);
  const bool is_first = !is_restored_;
  is_restored_ = true;
  if (is_first) {
    // After the first layout, so the panes have their sizes to share.
    QTimer::singleShot(0, this, [this] { RestoreSizes(); });
  }
}

}  // namespace snapper
