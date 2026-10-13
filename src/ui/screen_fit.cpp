#include "ui/screen_fit.h"

#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QMainWindow>
#include <QScreen>
#include <QSettings>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <utility>

#include "ui/panel_kit.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// The strip along a window's top that must lie on a screen for it to be
// grabbed and moved: its height, and its inset from the corners.
constexpr int kGrabStrip = 24;
constexpr int kGrabInset = 32;

QRect AvailableOf(const QWidget* widget) {
  assert(widget != nullptr);
  const QScreen* screen = widget->screen();
  const bool has_screen = screen != nullptr;
  const QScreen* used =
      has_screen ? screen : QGuiApplication::primaryScreen();
  assert(used != nullptr);
  return used->availableGeometry();
}

std::vector<QRect> AllScreens() {
  std::vector<QRect> screens;
  for (const QScreen* screen : QGuiApplication::screens()) {
    screens.push_back(screen->availableGeometry());
  }
  assert(screens.size() < 64);
  return screens;
}

// Fits the window again when it lands on a screen it does not fit.
class ScreenFollower final : public QObject {
 public:
  explicit ScreenFollower(QWidget* window) : QObject(window), window_(window) {
    assert(window_ != nullptr);
    assert(window_->isWindow());
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    assert(event != nullptr);
    const bool is_first_show = watched == window_ && !is_hooked_ &&
                               event->type() == QEvent::Show &&
                               window_->windowHandle() != nullptr;
    if (is_first_show) {
      is_hooked_ = true;
      connect(window_->windowHandle(), &QWindow::screenChanged, this,
              [this] { Refit(); });
    }
    return QObject::eventFilter(watched, event);
  }

 private:
  void Refit() const {
    assert(window_ != nullptr);
    const bool is_whole = window_->isMaximized() || window_->isFullScreen();
    const bool is_fitting =
        IsShownEnough(window_->frameGeometry(), {AvailableOf(window_)});
    if (!is_whole && !is_fitting) {
      window_->setGeometry(FittedRect(AvailableOf(window_)));
    }
  }

  QWidget* window_;
  bool is_hooked_ = false;
};

// Saves a window's layout when it closes.
class LayoutKeeper final : public QObject {
 public:
  LayoutKeeper(QMainWindow* window, QString settings_path, QString key)
      : QObject(window), window_(window),
        settings_path_(std::move(settings_path)), key_(std::move(key)) {
    assert(window_ != nullptr);
    assert(!settings_path_.isEmpty() && !key_.isEmpty());
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    assert(event != nullptr);
    const bool is_closing =
        watched == window_ && event->type() == QEvent::Close;
    if (is_closing) {
      QSettings settings(settings_path_, QSettings::IniFormat);
      settings.setValue(key_ + QStringLiteral("/geometry"),
                        window_->saveGeometry());
      settings.setValue(key_ + QStringLiteral("/docks"),
                        window_->saveState());
    }
    return QObject::eventFilter(watched, event);
  }

 private:
  QMainWindow* window_;
  QString settings_path_;
  QString key_;
};

// Tabs one dock behind another while the window is portrait.
class PortraitRule final : public QObject {
 public:
  PortraitRule(QMainWindow* window, QDockWidget* side, QDockWidget* main)
      : QObject(window), window_(window), side_(side), main_(main) {
    assert(window_ != nullptr);
    assert(side_ != nullptr && main_ != nullptr && side_ != main_);
  }

  void Arrange() {
    assert(window_ != nullptr);
    assert(side_ != nullptr);
    const bool is_portrait = IsPortrait(window_->size());
    const bool is_same = is_arranged_ && is_portrait == is_portrait_;
    if (is_same) {
      return;
    }
    is_arranged_ = true;
    is_portrait_ = is_portrait;
    if (is_portrait) {
      window_->tabifyDockWidget(main_, side_);
      main_->raise();
      return;
    }
    const bool is_tabbed = !window_->tabifiedDockWidgets(side_).isEmpty();
    if (is_tabbed) {
      window_->addDockWidget(Qt::LeftDockWidgetArea, side_);
    }
  }

 protected:
  bool eventFilter(QObject* watched, QEvent* event) override {
    assert(event != nullptr);
    const bool is_resized =
        watched == window_ && event->type() == QEvent::Resize;
    if (is_resized) {
      Arrange();
    }
    return QObject::eventFilter(watched, event);
  }

 private:
  QMainWindow* window_;
  QDockWidget* side_;
  QDockWidget* main_;
  bool is_arranged_ = false;
  bool is_portrait_ = false;
};

}  // namespace

QRect FittedRect(const QRect& available, double share) {
  assert(available.isValid());
  assert(share > 0.0 && share <= 1.0);
  const QSize size(static_cast<int>(available.width() * share),
                   static_cast<int>(available.height() * share));
  QRect fitted(QPoint(0, 0), size);
  fitted.moveCenter(available.center());
  return fitted;
}

std::vector<int> GoldenDockWidths(int width, int count) {
  assert(width >= 0);
  assert(count >= 0 && count < 16);
  if (count == 0) {
    return {};
  }
  const int sides = GoldenSplit(width).short_part;
  return std::vector<int>(static_cast<size_t>(count), sides / count);
}

bool IsPortrait(const QSize& size) {
  assert(size.width() >= 0);
  assert(size.height() >= 0);
  const bool is_empty = size.height() == 0;
  return !is_empty &&
         static_cast<double>(size.width()) / size.height() < kPortraitRatio;
}

bool IsShownEnough(const QRect& frame, const std::vector<QRect>& screens) {
  assert(screens.size() < 64);
  assert(kGrabInset > 0 && kGrabStrip > 0);
  if (!frame.isValid()) {
    return false;
  }
  const int inset = std::min(kGrabInset, frame.width() / 4);
  const QRect strip(frame.left() + inset, frame.top(),
                    frame.width() - 2 * inset, kGrabStrip);
  for (const QRect& screen : screens) {
    const bool is_grabbable = screen.intersects(strip);
    const bool is_small_enough = frame.width() <= screen.width() &&
                                 frame.height() <= screen.height();
    if (is_grabbable && is_small_enough) {
      return true;
    }
  }
  return false;
}

QSize DialogSize(const QSizeF& em_size, int em, const QRect& available) {
  assert(em > 0);
  assert(em_size.width() > 0.0 && em_size.height() > 0.0);
  const QSize wanted(static_cast<int>(std::lround(em_size.width() * em)),
                     static_cast<int>(std::lround(em_size.height() * em)));
  const QSize most(static_cast<int>(available.width() * kDialogShare),
                   static_cast<int>(available.height() * kDialogShare));
  return wanted.boundedTo(most);
}

void FitWindowToScreen(QWidget* window) {
  assert(window != nullptr);
  assert(window->isWindow());
  window->setGeometry(FittedRect(AvailableOf(window)));
  auto follower = std::make_unique<ScreenFollower>(window);
  window->installEventFilter(follower.get());
  follower.release();
}

void GoldenDocks(QMainWindow* window, const std::vector<QDockWidget*>& docks) {
  assert(window != nullptr);
  assert(!docks.empty());
  const std::vector<int> widths =
      GoldenDockWidths(window->width(), static_cast<int>(docks.size()));
  window->resizeDocks(QList<QDockWidget*>(docks.begin(), docks.end()),
                      QList<int>(widths.begin(), widths.end()),
                      Qt::Horizontal);
}

bool RememberLayout(QMainWindow* window, const QString& settings_path,
                    const QString& key) {
  assert(window != nullptr);
  assert(!key.isEmpty() && !key.contains('/'));
  const bool is_kept = !settings_path.isEmpty();
  if (!is_kept) {
    FitWindowToScreen(window);
    return false;
  }
  const QSettings settings(settings_path, QSettings::IniFormat);
  const QByteArray geometry =
      settings.value(key + QStringLiteral("/geometry")).toByteArray();
  const bool is_placed = !geometry.isEmpty() &&
                         window->restoreGeometry(geometry) &&
                         IsShownEnough(window->geometry(), AllScreens());
  if (!is_placed) {
    FitWindowToScreen(window);
  } else {
    // Put back, it still fits again when it moves to another screen.
    auto follower = std::make_unique<ScreenFollower>(window);
    window->installEventFilter(follower.get());
    follower.release();
  }
  const QByteArray docks =
      settings.value(key + QStringLiteral("/docks")).toByteArray();
  const bool has_docks = !docks.isEmpty();
  if (has_docks) {
    window->restoreState(docks);
  }
  auto keeper = std::make_unique<LayoutKeeper>(window, settings_path, key);
  window->installEventFilter(keeper.get());
  keeper.release();
  return is_placed;
}

void FitDialog(QWidget* dialog, const QSizeF& em_size) {
  assert(dialog != nullptr);
  assert(em_size.isValid());
  const QRect available = AvailableOf(dialog);
  const QSize size =
      DialogSize(em_size, theme::Em(dialog), available)
          .expandedTo(dialog->minimumSizeHint())
          .boundedTo(available.size());
  QRect place(QPoint(0, 0), size);
  const QWidget* parent = dialog->parentWidget();
  const bool has_parent = parent != nullptr;
  place.moveCenter(has_parent ? parent->window()->frameGeometry().center()
                              : available.center());
  dialog->setGeometry(place);
}

void TabDocksWhenPortrait(QMainWindow* window, QDockWidget* side,
                          QDockWidget* main) {
  assert(window != nullptr);
  assert(side != nullptr && main != nullptr);
  auto rule = std::make_unique<PortraitRule>(window, side, main);
  window->installEventFilter(rule.get());
  rule->Arrange();
  rule.release();
}

}  // namespace snapper
