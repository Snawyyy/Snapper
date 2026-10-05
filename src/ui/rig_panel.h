#ifndef SNAPPER_UI_RIG_PANEL_H_
#define SNAPPER_UI_RIG_PANEL_H_

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/managers.h"

namespace snapper {

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
  void Refresh();

 signals:
  void DollPicked(const QString& doll);
  void PiecePicked(const QString& piece);
  void Problem(const QString& why);

 private:
  void BuildLayout();
  void Wire();
  void WireChains();
  void RefreshPieces();
  void RefreshPiece();
  void RefreshChains();
  void AddChain();

  Managers managers_;
  QString doll_;
  QString piece_;
  QVBoxLayout layout_;
  QComboBox dolls_;
  QListWidget pieces_;
  QFormLayout form_;
  QComboBox parent_;
  QSpinBox order_;
  QDoubleSpinBox rest_;
  QComboBox drawing_;
  QSpinBox warp_columns_;
  QSpinBox warp_rows_;
  QLabel chains_title_;
  QListWidget chains_;
  QHBoxLayout chain_buttons_;
  QPushButton add_chain_;
  QPushButton flip_chain_;
  QPushButton remove_chain_;
  QPushButton save_rig_;
  QLabel hint_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_PANEL_H_
