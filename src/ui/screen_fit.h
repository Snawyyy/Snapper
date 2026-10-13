#ifndef SNAPPER_UI_SCREEN_FIT_H_
#define SNAPPER_UI_SCREEN_FIT_H_

#include <QRect>
#include <QSize>
#include <QSizeF>
#include <QString>

#include <vector>

class QDockWidget;
class QMainWindow;
class QWidget;

namespace snapper {

// Fitting windows to whatever screen they are on: any size, any shape.
// The pure parts (FittedRect, GoldenDockWidths, IsPortrait,
// IsShownEnough, DialogSize) take plain numbers; the rest apply them.

// How much of the screen a window takes when it opens fresh.
inline constexpr double kWindowShare = 0.85;
// The most of the screen a dialog takes.
inline constexpr double kDialogShare = 0.9;
// A window narrower than this many times its height is portrait.
inline constexpr double kPortraitRatio = 1.2;

// A rect share of available, centred in it.
QRect FittedRect(const QRect& available, double share = kWindowShare);

// The widths of count docks beside a centre that takes the golden
// ratio's long part of width; the docks share the short part equally.
std::vector<int> GoldenDockWidths(int width, int count);

// Whether a window of size is too narrow for docks on both sides
// (width / height under kPortraitRatio).
bool IsPortrait(const QSize& size);

// Whether a window at frame can be seen and grabbed on one of screens
// (their available rects): its title strip lies on one and it is no
// bigger than that screen. A saved place on a monitor that is gone is
// not.
bool IsShownEnough(const QRect& frame, const std::vector<QRect>& screens);

// em_size in ems (see theme::Em) as pixels, no bigger than kDialogShare
// of available.
QSize DialogSize(const QSizeF& em_size, int em, const QRect& available);

// Sizes window to kWindowShare of its screen, centred, and fits it again
// whenever it moves to another screen (unless maximised or full screen).
void FitWindowToScreen(QWidget* window);

// Gives the stage the golden ratio's long part of window's width and
// splits the rest between docks.
void GoldenDocks(QMainWindow* window, const std::vector<QDockWidget*>& docks);

// Puts window back where it was (size, place, docks) from the settings
// file under key, and saves it there whenever it closes. A saved place
// that is off every screen is refused, and the window is fitted instead.
// Returns whether it was put back. An empty settings_path remembers
// nothing and only fits.
bool RememberLayout(QMainWindow* window, const QString& settings_path,
                    const QString& key);

// Sizes dialog to em_size ems, capped to its screen, centred on its
// parent (or the screen).
void FitDialog(QWidget* dialog, const QSizeF& em_size);

// The portrait rule: while window is portrait, side is tabbed with main
// (Cast behind the Inspector); when it widens again, side goes back to
// its own column on the left. Follows every resize.
void TabDocksWhenPortrait(QMainWindow* window, QDockWidget* side,
                          QDockWidget* main);

}  // namespace snapper

#endif  // SNAPPER_UI_SCREEN_FIT_H_
