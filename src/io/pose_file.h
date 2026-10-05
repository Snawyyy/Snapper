#ifndef SNAPPER_IO_POSE_FILE_H_
#define SNAPPER_IO_POSE_FILE_H_

#include <QString>

#include <map>
#include <vector>

#include "base/error.h"
#include "model/pose.h"

namespace snapper {

constexpr int kMaxSavedPoses = 1000;

// A named doll pose kept for reuse across projects.
struct SavedPose final {
  QString name;
  // The doll it was made on; it fits any doll with the same pieces.
  QString doll;
  std::map<QString, PiecePose> pieces;

  bool operator==(const SavedPose&) const = default;
};

// A missing file is an empty list: nothing saved yet.
Result<std::vector<SavedPose>> ReadPoses(const QString& path);
Result<void> WritePoses(const QString& path,
                        const std::vector<SavedPose>& poses);

}  // namespace snapper

#endif  // SNAPPER_IO_POSE_FILE_H_
