#ifndef SNAPPER_IO_OBJECT_FILE_H_
#define SNAPPER_IO_OBJECT_FILE_H_

#include <QImage>
#include <QString>
#include <QStringList>

#include <vector>

#include "base/error.h"
#include "model/doll.h"

namespace snapper {

// A picture read to become an object: its frames, all one size, and
// each one's own time.
struct ObjectFrames final {
  std::vector<QImage> frames;
  // Milliseconds per frame, as the file had them (0 when it had none).
  std::vector<int> frame_ms;
  // Frames the file had; more than kMaxObjectFrames keeps every Nth.
  int source_count = 0;
  // N: 1 when every frame was kept.
  int kept_every = 1;
};

// True when path names a picture an object can be made from (PNG, GIF,
// WebP, JPEG, BMP), by its suffix.
bool IsObjectPicture(const QString& path);
// The pictures paths name, a folder giving every picture in it, sorted.
QStringList ObjectPictures(const QStringList& paths);

// Reads every frame of the picture at path (one for a still). Past
// kMaxObjectFrames it keeps every Nth frame, adding the times of those
// left out to the one kept, so it plays as long.
Result<ObjectFrames> ReadObjectFrames(const QString& path);

// Writes frames as an object's doll folder: one PNG a frame ("frame
// 1.png"...), art.json with one piece called name holding them, centred
// on the doll's origin, and rig.json marking it an object (ObjectClip),
// jointed at its middle, Smooth.
Result<void> WriteObjectFolder(const QString& folder, const QString& name,
                               const ObjectFrames& frames);

}  // namespace snapper

#endif  // SNAPPER_IO_OBJECT_FILE_H_
