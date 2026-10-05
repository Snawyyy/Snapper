#include "render/image_cache.h"

#include <cassert>

namespace snapper {

const QImage& ImageCache::Get(const QString& path) {
  assert(bytes_ >= 0);
  assert(images_.size() == uses_.size());
  const auto found = images_.find(path);
  const bool is_cached = found != images_.end();
  if (is_cached) {
    uses_.splice(uses_.begin(), uses_, found->second.use);
    return found->second.image;
  }
  QImage image(path);
  const bool is_loaded = !image.isNull();
  if (is_loaded) {
    image.convertTo(QImage::Format_ARGB32_Premultiplied);
  }
  bytes_ += image.sizeInBytes();
  uses_.push_front(path);
  Entry& entry = images_[path];
  entry = {std::move(image), uses_.begin()};
  Trim();
  return images_.at(path).image;
}

void ImageCache::Forget(const QString& path) {
  assert(bytes_ >= 0);
  assert(images_.size() == uses_.size());
  const auto found = images_.find(path);
  const bool is_cached = found != images_.end();
  if (is_cached) {
    bytes_ -= found->second.image.sizeInBytes();
    uses_.erase(found->second.use);
    images_.erase(found);
  }
}

void ImageCache::Trim() {
  assert(images_.size() == uses_.size());
  assert(!uses_.empty());
  // Never drops the image just asked for, even if it alone is too big.
  const size_t limit = uses_.size();
  for (size_t i = 1; i < limit && bytes_ > kImageCacheBytes; ++i) {
    const QString oldest = uses_.back();
    Forget(oldest);
  }
}

}  // namespace snapper
