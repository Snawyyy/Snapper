#include "ui/skin_tiles.h"

#include <QApplication>

#include <algorithm>
#include <cassert>

namespace snapper {
namespace {

constexpr int kColumns = 3;

}  // namespace

SkinTiles::SkinTiles(const QString& hint, QWidget* parent)
    : QWidget(parent), grid_(this), hint_(hint) {
  grid_.setContentsMargins(0, 0, 0, 0);
  grid_.setSpacing(4);
  assert(tiles_.empty());
  assert(kColumns > 0);
}

void SkinTiles::Show(const std::vector<QString>& names,
                     const std::vector<QString>& lit) {
  assert(names.size() < 1000);
  assert(lit.size() < 1000);
  const bool is_new = names != names_;
  if (is_new) {
    for (QPushButton* tile : tiles_) {
      grid_.removeWidget(tile);
      tile->deleteLater();
    }
    tiles_.clear();
    names_ = names;
    for (size_t i = 0; i < names.size(); ++i) {
      const QString name = names[i];
      auto* tile = new QPushButton(name, this);
      tile->setCheckable(true);
      tile->setContextMenuPolicy(Qt::CustomContextMenu);
      tile->setToolTip(hint_);
      connect(tile, &QPushButton::clicked, this, [this, name] {
        emit Clicked(name, QApplication::keyboardModifiers());
      });
      connect(tile, &QWidget::customContextMenuRequested, this,
              [this, name, tile](const QPoint& at) {
                emit MenuAsked(name, tile->mapToGlobal(at));
              });
      const int at = static_cast<int>(i);
      grid_.addWidget(tile, at / kColumns, at % kColumns);
      tiles_.push_back(tile);
    }
  }
  for (size_t i = 0; i < tiles_.size(); ++i) {
    const bool is_lit = std::ranges::find(lit, names_[i]) != lit.end();
    tiles_[i]->setChecked(is_lit);
  }
  assert(tiles_.size() == names_.size());
}

}  // namespace snapper
