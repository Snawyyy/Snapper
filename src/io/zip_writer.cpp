#include "io/zip_writer.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

namespace snapper {
namespace {

constexpr uint32_t kLocalHeader = 0x04034b50U;
constexpr uint32_t kCentralHeader = 0x02014b50U;
constexpr uint32_t kEndRecord = 0x06054b50U;
constexpr uint16_t kVersion = 20;
// Bit 11: names are UTF-8.
constexpr uint16_t kUtf8Names = 0x0800U;
// 1980-01-01 00:00 in DOS time, so the same doll gives the same bytes.
constexpr uint16_t kDosTime = 0;
constexpr uint16_t kDosDate = (0U << 9) | (1U << 5) | 1U;

constexpr std::array<uint32_t, 256> MakeCrcTable() {
  std::array<uint32_t, 256> table{};
  for (uint32_t n = 0; n < 256; ++n) {
    uint32_t c = n;
    for (int bit = 0; bit < 8; ++bit) {
      c = (c & 1U) != 0 ? 0xEDB88320U ^ (c >> 1) : c >> 1;
    }
    table[n] = c;
  }
  return table;
}

constexpr std::array<uint32_t, 256> kCrcTable = MakeCrcTable();

uint32_t Crc32(const QByteArray& data) {
  uint32_t crc = 0xFFFFFFFFU;
  for (const char byte : data) {
    const uint32_t index = (crc ^ static_cast<uint8_t>(byte)) & 0xFFU;
    crc = kCrcTable[index] ^ (crc >> 8);
  }
  return crc ^ 0xFFFFFFFFU;
}

void Put16(QByteArray* out, uint16_t value) {
  assert(out != nullptr);
  out->append(static_cast<char>(value & 0xFFU));
  out->append(static_cast<char>((value >> 8) & 0xFFU));
}

void Put32(QByteArray* out, uint32_t value) {
  assert(out != nullptr);
  Put16(out, static_cast<uint16_t>(value & 0xFFFFU));
  Put16(out, static_cast<uint16_t>(value >> 16));
}

uint32_t Size32(qsizetype size) {
  assert(size >= 0);
  assert(static_cast<uint64_t>(size) < std::numeric_limits<uint32_t>::max());
  return static_cast<uint32_t>(size);
}

// The fields a local header and a central header share, from the
// version needed to the extra field's length.
void PutCommon(QByteArray* out, const QByteArray& name, uint32_t crc,
               uint32_t size) {
  assert(out != nullptr);
  assert(name.size() <= std::numeric_limits<uint16_t>::max());
  Put16(out, kVersion);
  Put16(out, kUtf8Names);
  Put16(out, 0);  // Stored, no compression.
  Put16(out, kDosTime);
  Put16(out, kDosDate);
  Put32(out, crc);
  Put32(out, size);
  Put32(out, size);
  Put16(out, static_cast<uint16_t>(name.size()));
  Put16(out, 0);  // No extra field.
}

}  // namespace

QByteArray StoredZip(const std::vector<ZipEntry>& entries) {
  assert(entries.size() <= static_cast<size_t>(kMaxZipEntries));
  QByteArray out;
  QByteArray central;
  for (const ZipEntry& entry : entries) {
    assert(!entry.name.isEmpty());
    const QByteArray name = entry.name.toUtf8();
    const uint32_t crc = Crc32(entry.data);
    const uint32_t size = Size32(entry.data.size());
    const uint32_t offset = Size32(out.size());
    Put32(&out, kLocalHeader);
    PutCommon(&out, name, crc, size);
    out.append(name);
    out.append(entry.data);
    Put32(&central, kCentralHeader);
    Put16(&central, kVersion);  // Made by.
    PutCommon(&central, name, crc, size);
    Put16(&central, 0);  // No comment.
    Put16(&central, 0);  // Disk 0.
    Put16(&central, 0);  // Internal attributes.
    Put32(&central, 0);  // External attributes.
    Put32(&central, offset);
    central.append(name);
  }
  const uint32_t central_at = Size32(out.size());
  const auto count = static_cast<uint16_t>(entries.size());
  out.append(central);
  Put32(&out, kEndRecord);
  Put16(&out, 0);  // This disk.
  Put16(&out, 0);  // Disk with the central directory.
  Put16(&out, count);
  Put16(&out, count);
  Put32(&out, Size32(central.size()));
  Put32(&out, central_at);
  Put16(&out, 0);  // No comment.
  assert(out.size() >= central.size());
  return out;
}

}  // namespace snapper
