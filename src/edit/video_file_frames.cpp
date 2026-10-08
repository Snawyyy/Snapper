#include "edit/video_file_frames.h"

#include <algorithm>
#include <cassert>

namespace snapper {

QImage VideoFileFrames::Picture(const QString& path, Frame frame) {
  assert(frame.index() >= 0);
  assert(readers_.size() <= static_cast<size_t>(kMaxOpenVideos));
  const bool is_known_broken = path.isEmpty() || broken_.contains(path);
  if (is_known_broken) {
    return QImage();
  }
  VideoReader* reader = ReaderFor(path);
  const bool is_open = reader != nullptr;
  if (!is_open) {
    return QImage();
  }
  auto picture = reader->PictureAt(SecondsAtFrame(frame));
  return picture.has_value() ? *picture : QImage();
}

void VideoFileFrames::Forget() {
  readers_.clear();
  last_use_.clear();
  broken_.clear();
  assert(readers_.empty());
  assert(broken_.empty());
}

VideoReader* VideoFileFrames::ReaderFor(const QString& path) {
  assert(!path.isEmpty());
  assert(!broken_.contains(path));
  last_use_[path] = ++uses_;
  const auto found = readers_.find(path);
  const bool is_open = found != readers_.end();
  if (is_open) {
    return found->second.get();
  }
  const bool is_full = readers_.size() >= static_cast<size_t>(kMaxOpenVideos);
  if (is_full) {
    const auto oldest = std::ranges::min_element(
        readers_, {}, [this](const auto& entry) {
          return last_use_.at(entry.first);
        });
    last_use_.erase(oldest->first);
    readers_.erase(oldest);
  }
  auto reader = std::make_unique<VideoReader>();
  const bool is_opened = reader->Open(path).has_value();
  if (!is_opened) {
    broken_.insert(path);
    last_use_.erase(path);
    return nullptr;
  }
  VideoReader* raw = reader.get();
  readers_[path] = std::move(reader);
  return raw;
}

}  // namespace snapper
