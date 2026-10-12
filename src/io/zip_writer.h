#ifndef SNAPPER_IO_ZIP_WRITER_H_
#define SNAPPER_IO_ZIP_WRITER_H_

#include <QByteArray>
#include <QString>

#include <vector>

namespace snapper {

// One file inside a zip.
struct ZipEntry final {
  QString name;
  QByteArray data;
};

// The most entries a zip Snapper writes may hold (pieces times drawings
// plus a few), so a bad call cannot grow it without end.
constexpr int kMaxZipEntries = 16384;

// A zip archive of entries, stored without compression and in the
// order given (OpenRaster needs "mimetype" first and uncompressed; the
// PNGs inside are compressed already).
QByteArray StoredZip(const std::vector<ZipEntry>& entries);

}  // namespace snapper

#endif  // SNAPPER_IO_ZIP_WRITER_H_
