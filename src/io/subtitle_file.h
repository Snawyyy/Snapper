#ifndef SNAPPER_IO_SUBTITLE_FILE_H_
#define SNAPPER_IO_SUBTITLE_FILE_H_

#include <QString>

#include <vector>

#include "base/error.h"
#include "base/frame.h"

namespace snapper {

// One timed line: shown from start up to (not including) end.
struct SubtitleCue final {
  Frame start;
  Frame end;
  QString text;

  bool operator==(const SubtitleCue&) const = default;
};

// The cues of an SRT or WebVTT file's text (told apart by its WEBVTT
// header), times rounded to the nearest frame, the file's own styling
// (tags, cue settings) dropped. Cues without words or time are skipped;
// a file with none is refused.
Result<std::vector<SubtitleCue>> ParseSubtitles(const QString& content);
// cues as an SRT or a WebVTT file's text.
QString SrtText(const std::vector<SubtitleCue>& cues);
QString VttText(const std::vector<SubtitleCue>& cues);

}  // namespace snapper

#endif  // SNAPPER_IO_SUBTITLE_FILE_H_
