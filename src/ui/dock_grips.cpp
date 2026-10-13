#include "ui/dock_grips.h"

#include <QCursor>
#include <QDockWidget>
#include <QEvent>
#include <QMainWindow>
#include <QSettings>

#include <algorithm>
#include <cassert>

#include "ui/screen_fit.h"
#include "ui/snapper_style.h"

namespace snapper {
namespace {

constexpr int kTickMs = 40;
constexpr int kLeastWidth = 120;
// No dock may be restored wider than this share of the window.
constexpr double kMostShare = 0.45;
constexpr int kMaxDocks = 8;

QString KeyOf(const QDockWidget& dock) {
  assert(!dock.objectName().isEmpty());
  assert(!dock.objectName().contains('/'));
  return QStringLiteral("docks/") + dock.objectName();
}

}  // namespace

DockGrips::DockGrips(QMainWindow* window, std::vector<QDockWidget*> docks,
                     const QString& settings_path)
    : QObject(window),
      window_(window),
      docks_(std::move(docks)),
      settings_path_(settings_path) {
  assert(window_ != nullptr);
  assert(!docks_.empty() && docks_.size() <= kMaxDocks);
  window_->setMouseTracking(true);
  window_->installEventFilter(this);
  tick_.setInterval(kTickMs);
  connect(&tick_, &QTimer::timeout, this, [this] { Follow(); });
}

QRect DockGrips::SeparatorAt(const QPoint& at, QDockWidget** dock) const {
  assert(dock != nullptr);
  assert(window_ != nullptr);
  const QWidget* centre = window_->centralWidget();
  const bool has_centre = centre != nullptr;
  if (!has_centre) {
    return QRect();
  }
  const QRect middle = centre->geometry();
  for (QDockWidget* candidate : docks_) {
    const bool is_docked = candidate->isVisible() && !candidate->isFloating();
    if (!is_docked) {
      continue;
    }
    const QRect box = candidate->geometry();
    const bool is_left =
        window_->dockWidgetArea(candidate) == Qt::LeftDockWidgetArea;
    const int left = is_left ? box.right() + 1 : middle.right() + 1;
    const int right = is_left ? middle.left() - 1 : box.left() - 1;
    // The gap is the separator; a pixel of slack each side.
    const QRect gap(QPoint(left - 1, box.top()),
                    QPoint(right + 1, box.bottom()));
    const bool is_on = gap.isValid() && gap.contains(at);
    if (is_on) {
      *dock = candidate;
      return gap;
    }
  }
  return QRect();
}

void DockGrips::Publish(bool is_lit) {
  assert(window_ != nullptr);
  assert(!is_lit || !over_.isNull());
  is_lit_ = is_lit;
  window_->setProperty("litGrip", is_lit ? over_ : QRect());
  window_->update();
}

void DockGrips::Follow() {
  assert(window_ != nullptr);
  assert(kGripDwellMs > 0);
  QDockWidget* dock = nullptr;
  const QPoint at = window_->mapFromGlobal(QCursor::pos());
  const QRect now = SeparatorAt(at, &dock);
  const bool is_on = !now.isNull();
  if (is_down_) {
    // Dragging: the separator moves with the pointer and stays lit.
    over_ = is_on ? now : over_;
    Publish(true);
    return;
  }
  if (!is_on) {
    over_ = QRect();
    tick_.stop();
    Publish(false);
    return;
  }
  const bool is_new = now != over_;
  if (is_new) {
    over_ = now;
    rested_.start();
    Publish(false);
  }
  const bool is_ready = rested_.elapsed() >= kGripDwellMs && !is_lit_;
  if (is_ready) {
    Publish(true);
  }
  const bool is_idle = !tick_.isActive();
  if (is_idle) {
    tick_.start();
  }
}

int DockGrips::DefaultWidth() const {
  assert(!docks_.empty());
  assert(window_->width() >= 0);
  return GoldenDockWidths(window_->width(), 2).front();
}

void DockGrips::ResetWidth(QDockWidget* dock) {
  assert(dock != nullptr);
  assert(window_ != nullptr);
  window_->resizeDocks({dock}, {DefaultWidth()}, Qt::Horizontal);
  const bool is_kept = !settings_path_.isEmpty();
  if (is_kept) {
    QSettings settings(settings_path_, QSettings::IniFormat);
    settings.remove(KeyOf(*dock));
  }
}

void DockGrips::SaveWidths() const {
  assert(window_ != nullptr);
  assert(docks_.size() <= kMaxDocks);
  const bool is_kept = !settings_path_.isEmpty();
  if (!is_kept) {
    return;
  }
  QSettings settings(settings_path_, QSettings::IniFormat);
  for (const QDockWidget* dock : docks_) {
    const bool is_docked = dock->isVisible() && !dock->isFloating();
    if (is_docked) {
      settings.setValue(KeyOf(*dock), dock->width());
    }
  }
}

void DockGrips::RestoreWidths() {
  assert(window_ != nullptr);
  assert(docks_.size() <= kMaxDocks);
  const bool is_kept = !settings_path_.isEmpty();
  if (!is_kept) {
    return;
  }
  const QSettings settings(settings_path_, QSettings::IniFormat);
  const int most = static_cast<int>(window_->width() * kMostShare);
  for (QDockWidget* dock : docks_) {
    const int saved = settings.value(KeyOf(*dock), 0).toInt();
    const bool has_saved = saved > 0 && dock->isVisible();
    if (has_saved) {
      window_->resizeDocks({dock}, {std::clamp(saved, kLeastWidth, most)},
                           Qt::Horizontal);
    }
  }
}

bool DockGrips::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(window_ != nullptr);
  const bool is_window = watched == window_;
  const QEvent::Type type = event->type();
  const bool is_mouse = type == QEvent::MouseMove ||
                        type == QEvent::MouseButtonPress ||
                        type == QEvent::MouseButtonRelease ||
                        type == QEvent::MouseButtonDblClick;
  const bool is_first_show = type == QEvent::Show && !is_restored_;
  const bool is_restoring = is_window && is_first_show;
  if (is_restoring) {
    is_restored_ = true;
    // After the first layout, so the docks have their sizes.
    QTimer::singleShot(0, this, [this] { RestoreWidths(); });
  }
  const bool is_ours = is_window && is_mouse;
  if (!is_ours) {
    return QObject::eventFilter(watched, event);
  }
  QDockWidget* dock = nullptr;
  const QPoint at = window_->mapFromGlobal(QCursor::pos());
  const bool is_on = !SeparatorAt(at, &dock).isNull();
  const bool is_double = type == QEvent::MouseButtonDblClick && is_on;
  if (is_double) {
    ResetWidth(dock);
    return true;
  }
  const bool was_down = is_down_;
  is_down_ = type == QEvent::MouseButtonPress ? is_on
             : type == QEvent::MouseButtonRelease ? false
                                                   : is_down_;
  const bool has_let_go = was_down && !is_down_;
  if (has_let_go) {
    SaveWidths();
  }
  Follow();
  return QObject::eventFilter(watched, event);
}

}  // namespace snapper
