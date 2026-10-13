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
#include "ui/collider_box.h"
#include "ui/live_edit.h"
#include "ui/look_box.h"
#include "ui/managers.h"
#include "ui/skin_rig_box.h"

namespace snapper {

// An old warp grid shows 3 by 3 when its fields are first shown.
constexpr int kDefaultWarpCells = 3;

// The rig's settings beside the rig canvas, as plated sections in the
// Pose box style (panel_kit.h), each shown only when the pick needs it:
// Doll (its pieces as a tree, saving the rig back to the
// library), Look, Skins and IK chains with a doll; Joint (parent, rest
// turn, draw order, drawing, mirroring) and Warp with a piece; Collider
// while the piece is one. While a skin is being changed the Drawing
// picker sets the picked pieces' drawing in that skin instead of the
// default. The piece list picks by the one pick rule (PickList).
class RigPanel final : public QWidget {
  Q_OBJECT

 public:
  explicit RigPanel(const Managers& managers);

  const QString& doll() const { return doll_; }
  const QString& piece() const { return piece_; }
  // Opens doll for rigging (a click on its card in the library); refused
  // quietly when the project has no such doll.
  void OpenDoll(const QString& doll);
  void PickPiece(const QString& piece);
  // Picks pieces with focus shown in the fields, as the canvas does.
  void SetPick(const QStringList& picked, const QString& focus);
  void Refresh();

 signals:
  void DollPicked(const QString& doll);
  // The skin being changed (empty for none), for the canvas to wear.
  void SkinPicked(const QString& skin);
  void PickChanged(const QStringList& picked, const QString& focus);
  void Problem(const QString& why);

 private:
  void BuildLayout();
  void SetUpWarpFields();
  void Wire();
  void WireWarp();
  void WireChains();
  void RefreshPieces();
  void RefreshPiece();
  // Draw order and rest turn of the focused piece; picks that differ
  // say Mixed.
  void ShowOrderAndRest(const Doll& doll, const RigPiece& focus);
  // Shows only the settings that fit: doll-wide ones with a doll, piece
  // ones with a piece (rig, or nullptr for none), and of the warp only
  // the fields its kind uses.
  void ShowOnlyRelevant(bool has_doll, const RigPiece* rig);
  // The warp fields of the focused piece (nullptr for none): which are
  // live, and why the others are greyed.
  void ShowWarp(const RigPiece* rig);
  // Gives the picked pieces a shape mesh at the Detail shown, saying
  // what carried over.
  void MakeShapeMesh();
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
  QListWidget pieces_;
  // The sections shown by what is picked, and the forms of two of them
  // (their widgets own them).
  QWidget* doll_section_ = nullptr;
  QWidget* joint_section_ = nullptr;
  QWidget* warp_section_ = nullptr;
  QWidget* chains_section_ = nullptr;
  QFormLayout* joint_form_ = nullptr;
  QFormLayout* warp_form_ = nullptr;
  QComboBox parent_;
  QSpinBox order_;
  QDoubleSpinBox rest_;
  QComboBox drawing_;
  QCheckBox warp_on_;
  QCheckBox keep_shape_;
  QHBoxLayout warp_row_;
  QSpinBox warp_columns_;
  QSpinBox warp_rows_;
  QComboBox detail_;
  QPushButton convert_;
  QListWidget chains_;
  QHBoxLayout chain_buttons_;
  QPushButton add_chain_;
  QPushButton flip_chain_;
  QPushButton remove_chain_;
  ColliderBox collider_box_;
  SkinRigBox skins_;
  LookBox look_;
  QCheckBox stays_crisp_;
  QHBoxLayout mirror_buttons_;
  QPushButton copy_piece_;
  QPushButton copy_side_;
  QPushButton mirror_nodes_;
  QPushButton save_rig_;
  QLabel hint_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_PANEL_H_
