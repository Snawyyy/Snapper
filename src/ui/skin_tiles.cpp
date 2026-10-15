#include "ui/skin_tiles.h"

#include <QApplication>

#include <algorithm>
#include <cassert>
#include <memory>

namespace snapper {

SkinTiles::SkinTiles(const QString& hint, QWidget* parent)
    : QWidget(parent), rack_(this), hint_(hint) {
  rack_.setContentsMargins(0, 0, 0, 0);
  rack_.setSpacing(1);
  assert(tiles_.empty());
  assert(rack_.count() == 0);
}

void SkinTiles::Show(const std::vector<QString>& names,
                     const std::vector<QString>& lit) {
  assert(names.size() < 1000);
  assert(lit.size() < 1000);
  const bool is_new = names != names_;
  if (is_new) {
    for (TapeSpine* tile : tiles_) {
      rack_.removeWidget(tile);
      tile->deleteLater();
    }
    tiles_.clear();
    names_ = names;
    for (size_t i = 0; i < names.size(); ++i) {
      const QString name = names[i];
      // The rack owns it.
      auto* tile = std::make_unique<TapeSpine>(name).release();
      tile->setParent(this);
      tile->setContextMenuPolicy(Qt::CustomContextMenu);
      tile->setToolTip(hint_);
      connect(tile, &QAbstractButton::clicked, this, [this, name] {
        emit Clicked(name, QApplication::keyboardModifiers());
      });
      connect(tile, &QWidget::customContextMenuRequested, this,
              [this, name, tile](const QPoint& at) {
                emit MenuAsked(name, tile->mapToGlobal(at));
              });
      rack_.addWidget(tile);
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
