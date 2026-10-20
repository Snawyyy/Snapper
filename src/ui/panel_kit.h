#ifndef SNAPPER_UI_PANEL_KIT_H_
#define SNAPPER_UI_PANEL_KIT_H_

#include <QFormLayout>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace snapper {

// The panel kit: the pieces every panel is built from, so all of them
// look like the Pose box. Headers come in two levels only: a section is
// a full-width raised title bar over one sunken well (a group box, a
// fold or PlatedSection); inside it, parts wear a quiet caption and no
// frame of their own, so no box sits in a box. Settings sit indented
// under their title; two things side by side share the width by the
// golden ratio. Square corners only, and no style sheets: the look
// comes from frames and the style (see theme.h, snapper_style.h).

// The golden ratio, and its two parts as whole shares of 100 for
// stretch factors (the long part 62, the short 38).
inline constexpr double kGoldenRatio = 1.6180339887498949;
inline constexpr int kGoldenLong = 62;
inline constexpr int kGoldenShort = 38;
// Every tab lays out the same: a side panel, the main view taking the
// golden ratio's long part, a side panel; each side takes half the
// short part, and never less than kSideMinWidth.
inline constexpr int kSideShare = kGoldenShort / 2;
inline constexpr int kMainShare = kGoldenLong;
inline constexpr int kSideMinWidth = 240;
// Space between tiles that share a row.
inline constexpr int kTileGap = 4;

// A length split by the golden ratio; the two parts add up to it.
struct GoldenParts final {
  int long_part = 0;
  int short_part = 0;
};
GoldenParts GoldenSplit(int total);

// A widget or layout its parent owns from here on.
template <typename T, typename... Args>
T* MakeOwned(Args&&... args) {
  static_assert(std::is_class_v<T>);
  static_assert(sizeof(T) > 0);
  return std::make_unique<T>(std::forward<Args>(args)...).release();
}

// Makes label a caption: the quiet second level header naming a part
// inside a section (the title of a control cell).
void MakePlate(QLabel* label);
// Makes label a title plate: raised, bold, text at the left, spanning
// the whole width, so it stands out over the settings indented under it.
void MakeTitlePlate(QLabel* label);

// control with bar right under it (when not null), both the same
// width, so the numbers read as part of the control. No border: it sits
// in its section's well.
QWidget* Framed(QWidget* control, QWidget* bar, QWidget* parent);
// Lays cell out as a column with no margin or spacing of its own: a
// title plate sits right on the frame under it.
QVBoxLayout* StackIn(QWidget* cell);
// A slot of a fixed height with widget centred in it, so cells that sit
// side by side line up whatever size their control is.
QWidget* InSlot(QWidget* widget, int height, QWidget* parent);

// A title plate at the edge over a sunken frame holding body, indented
// by kIndent: how a section of a panel is bordered. The plate is the
// section's child named "plate".
QWidget* PlatedSection(const QString& title, QWidget* body, QWidget* parent);

// Lays host out as a form the kit's way: labels left, fields growing,
// rows kRowGap / 2 apart, no margin (whatever holds it indents it).
QFormLayout* SettingsForm(QWidget* host);

// A caption with a form under it whose rows sit kIndent in, with no
// frame: a group of settings inside a section.
class SettingsGroup final : public QWidget {
  Q_OBJECT

 public:
  SettingsGroup(const QString& title, QWidget* parent);

  QFormLayout* form() { return &form_; }
  void SetTitle(const QString& title);

 private:
  QVBoxLayout layout_;
  QLabel title_;
  QWidget body_;
  QFormLayout form_;
};

// Cells side by side sharing the width by shares (one per cell, or
// empty for equal shares), gap pixels apart.
QWidget* SharedRow(const std::vector<QWidget*>& cells,
                   const std::vector<int>& shares, int gap, QWidget* parent);

// Two cells side by side, long_cell taking the golden ratio's long part
// of the width, short_cell the short one, kCellGap apart.
QWidget* GoldenRow(QWidget* long_cell, QWidget* short_cell, QWidget* parent);

// Tiles side by side in equal shares, kTileGap apart.
QWidget* TileRow(const std::vector<QWidget*>& tiles, QWidget* parent);

// A quiet, wrapped line that says why a list or panel is empty and what
// fills it ("Drop a picture here to add a reference.").
QLabel* EmptyHint(const QString& text, QWidget* parent);

}  // namespace snapper

#endif  // SNAPPER_UI_PANEL_KIT_H_
