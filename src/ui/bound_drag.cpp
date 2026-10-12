#include "ui/bound_drag.h"

#include <QSignalBlocker>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "ui/live_edit.h"

namespace snapper {

BoundDrag::BoundDrag(std::vector<QDoubleSpinBox*> boxes, LiveEdit* live)
    : boxes_(std::move(boxes)), starts_(boxes_.size(), 0.0), live_(live) {
  assert(!boxes_.empty());
  assert(std::none_of(boxes_.begin(), boxes_.end(),
                      [](const QDoubleSpinBox* box) {
                        return box == nullptr;
                      }));
}

double BoundDrag::value(size_t index) const {
  assert(index < boxes_.size());
  assert(boxes_[index] != nullptr);
  return boxes_[index]->value();
}

double BoundDrag::start(size_t index) const {
  assert(index < starts_.size());
  assert(std::isfinite(starts_[index]));
  return starts_[index];
}

double BoundDrag::low(size_t index) const {
  assert(index < boxes_.size());
  assert(boxes_[index] != nullptr);
  return boxes_[index]->minimum();
}

double BoundDrag::high(size_t index) const {
  assert(index < boxes_.size());
  assert(boxes_[index] != nullptr);
  return boxes_[index]->maximum();
}

bool BoundDrag::is_enabled() const {
  assert(!boxes_.empty());
  assert(boxes_.front() != nullptr);
  return boxes_.front()->isEnabled();
}

void BoundDrag::Begin() {
  assert(!is_active_);
  assert(starts_.size() == boxes_.size());
  for (size_t i = 0; i < boxes_.size(); ++i) {
    starts_[i] = boxes_[i]->value();
  }
  is_active_ = true;
}

void BoundDrag::Set(size_t index, double value) {
  assert(index < boxes_.size());
  assert(std::isfinite(value));
  QDoubleSpinBox* box = boxes_[index];
  box->setValue(std::clamp(value, box->minimum(), box->maximum()));
}

void BoundDrag::Finish() {
  assert(!boxes_.empty());
  assert(boxes_.front() != nullptr);
  const bool was_active = is_active_;
  is_active_ = false;
  if (was_active) {
    emit boxes_.front()->editingFinished();
  }
}

void BoundDrag::Cancel() {
  assert(starts_.size() == boxes_.size());
  assert(is_active_);
  is_active_ = false;
  const bool has_live = live_ != nullptr;
  if (has_live) {
    live_->Cancel();
  }
  for (size_t i = 0; i < boxes_.size(); ++i) {
    const QSignalBlocker quiet(boxes_[i]);
    boxes_[i]->setValue(starts_[i]);
  }
}

std::unique_ptr<HistoryManager::BurstGuard> BoundDrag::Burst() {
  assert(!boxes_.empty());
  assert(!is_active_);
  const bool has_live = live_ != nullptr;
  return has_live ? live_->Burst() : nullptr;
}

void BoundDrag::SetAll(const std::vector<double>& values) {
  assert(values.size() == boxes_.size());
  assert(!is_active_);
  Begin();
  for (size_t i = 0; i < values.size(); ++i) {
    Set(i, values[i]);
  }
  Finish();
}

}  // namespace snapper
