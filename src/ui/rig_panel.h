#ifndef SNAPPER_UI_RIG_PANEL_H_
#define SNAPPER_UI_RIG_PANEL_H_

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include <vector>

#include "model/doll.h"
#include "ui/live_edit.h"
#include "ui/managers.h"

namespace snapper {

// A new warp grid starts 3 by 3: enough to crease, few points to push.
constexpr int kDefaultWarpCells = 3;

// The rig's settings beside the rig canvas: which doll, its pieces as a
// tree, the picked piece's parent, draw order, default drawing and warp
// grid, its IK chains, and saving the rig back to the library.
class RigPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit RigPanel(const Managers& managers);

  const QString& doll() const { return doll_; }
  const QString& piece() const { return piece_; }
  void PickPiece(const QString& piece);
  // Picks pieces with focus shown in the fields, as the canvas does.
  void SetPick(const QStringList& picked, const QString& focus);
  void Refresh();

 signals:
  void DollPicked(const QString& doll);
  void PickChanged(const QStringList& picked, const QString& focus);
  void Problem(const QString& why);

 private:
  void BuildLayout();
  void Wire();
  void WireChains();
  void RefreshPieces();
  void RefreshPiece();
  void RefreshChains();
  void AddChain();
  // The picked pieces, at least the focused one.
  std::vector<QString> Picked() const;
  const RigPiece* Focus() const;

  Managers managers_;
  QString doll_;
  QString piece_;
  QStringList picked_;
  QVBoxLayout layout_;
  LiveEdit live_;
  QComboBox dolls_;
  QListWidget pieces_;
  QFormLayout form_;
  QComboBox parent_;
  QSpinBox order_;
  QDoubleSpinBox rest_;
  QComboBox drawing_;
  QLabel joint_title_;
  QLabel warp_title_;
  QCheckBox warp_on_;
  QHBoxLayout warp_row_;
  QSpinBox warp_columns_;
  QSpinBox warp_rows_;
  QLabel chains_title_;
  QListWidget chains_;
  QHBoxLayout chain_buttons_;
  QPushButton add_chain_;
  QPushButton flip_chain_;
  QPushButton remove_chain_;
  QHBoxLayout mirror_buttons_;
  QPushButton copy_piece_;
  QPushButton copy_side_;
  QPushButton save_rig_;
  QLabel hint_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_PANEL_H_
