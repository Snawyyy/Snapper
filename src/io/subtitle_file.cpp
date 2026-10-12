#include "io/subtitle_file.h"

#include <QRegularExpression>
#include <QStringList>

#include <cassert>
#include <cmath>
#include <optional>

#include "base/text.h"

namespace snapper {
namespace {

constexpr int kMaxCues = 100000;

// "01:02:03,450", "01:02:03.450" or "02:03.450" as seconds.
std::optional<double> SecondsOf(const QString& stamp) {
  assert(stamp.size() < 1000);
  static const QRegularExpression kStamp(
      QStringLiteral(R"(^(?:(\d+):)?(\d+):(\d+)(?:[,.](\d+))?$)"));
  assert(kStamp.isValid());
  const QRegularExpressionMatch match = kStamp.match(stamp.trimmed());
  const bool is_time = match.hasMatch();
  if (!is_time) {
    return std::nullopt;
  }
  const QString part = match.captured(4);
  const double fraction =
      part.isEmpty() ? 0.0
                     : part.toDouble() / std::pow(10.0, part.size());
  return match.captured(1).toDouble() * 3600.0 +
         match.captured(2).toDouble() * 60.0 + match.captured(3).toDouble() +
         fraction;
}

Frame NearestFrame(double seconds) {
  assert(std::isfinite(seconds));
  assert(kFramesPerSecond > 0);
  return Frame(static_cast<int>(std::lround(seconds * kFramesPerSecond)));
}

// The words of a cue without the file's styling: <i>, <c.red>, {\an8}.
QString PlainWords(const QStringList& lines) {
  assert(lines.size() < 100000);
  static const QRegularExpression kTags(QStringLiteral(R"(<[^>]*>|\{[^}]*\})"));
  assert(kTags.isValid());
  QStringList kept;
  for (const QString& line : lines) {
    kept.append(QString(line).remove(kTags).trimmed());
  }
  return kept.join(QLatin1Char('\n')).trimmed();
}

// One block of lines (between blank lines) as a cue, or nullopt when it
// holds no timing line or no words.
std::optional<SubtitleCue> CueOf(const QStringList& block) {
  assert(block.size() < 100000);
  assert(kMaxCues > 0);
  for (qsizetype i = 0; i < block.size(); ++i) {
    const qsizetype arrow = block[i].indexOf(QStringLiteral("-->"));
    if (arrow < 0) {
      continue;
    }
    // WebVTT may put cue settings after the end time.
    const QString after = block[i].mid(arrow + 3).trimmed();
    const auto start = SecondsOf(block[i].left(arrow));
    const auto end = SecondsOf(after.section(QLatin1Char(' '), 0, 0));
    const QString words = PlainWords(block.mid(i + 1));
    const bool is_cue = start && end && !words.isEmpty();
    if (!is_cue) {
      return std::nullopt;
    }
    const Frame from = NearestFrame(*start);
    const Frame to = std::max(NearestFrame(*end), Frame(from.index() + 1));
    return SubtitleCue{from, to, words};
  }
  return std::nullopt;
}

// "00:01:02,450" (SRT, comma) or "00:01:02.450" (WebVTT, dot).
QString StampOf(Frame frame, QChar mark) {
  assert(frame.index() >= 0);
  assert(!mark.isNull());
  const long long ms = std::llround(SecondsAtFrame(frame) * 1000.0);
  return QStringLiteral("%1:%2:%3%4%5")
      .arg(ms / 3600000, 2, 10, QLatin1Char('0'))
      .arg(ms / 60000 % 60, 2, 10, QLatin1Char('0'))
      .arg(ms / 1000 % 60, 2, 10, QLatin1Char('0'))
      .arg(mark)
      .arg(ms % 1000, 3, 10, QLatin1Char('0'));
}

}  // namespace

Result<std::vector<SubtitleCue>> ParseSubtitles(const QString& content) {
  assert(content.size() >= 0);
  assert(kMaxCues > 0);
  QString text = content;
  text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  std::vector<SubtitleCue> cues;
  QStringList block;
  const QStringList lines = text.split(QLatin1Char('\n'));
  for (qsizetype i = 0; i <= lines.size(); ++i) {
    const bool is_end = i == lines.size() || lines[i].trimmed().isEmpty();
    if (!is_end) {
      block.append(lines[i]);
      continue;
    }
    const auto cue = CueOf(block);
    block.clear();
    const bool has_room = static_cast<int>(cues.size()) < kMaxCues;
    if (cue && has_room) {
      cues.push_back(*cue);
    }
  }
  if (cues.empty()) {
    return std::unexpected(
        Error{Tr("No timed lines found: is it an SRT or WebVTT file?")});
  }
  return cues;
}

QString SrtText(const std::vector<SubtitleCue>& cues) {
  assert(cues.size() <= static_cast<size_t>(kMaxCues));
  QString out;
  int number = 1;
  for (const SubtitleCue& cue : cues) {
    assert(cue.start < cue.end);
    out += QStringLiteral("%1\n%2 --> %3\n%4\n\n")
               .arg(number)
               .arg(StampOf(cue.start, QLatin1Char(',')),
                    StampOf(cue.end, QLatin1Char(',')), cue.text);
    ++number;
  }
  return out;
}

QString VttText(const std::vector<SubtitleCue>& cues) {
  assert(cues.size() <= static_cast<size_t>(kMaxCues));
  QString out = QStringLiteral("WEBVTT\n\n");
  for (const SubtitleCue& cue : cues) {
    assert(cue.start < cue.end);
    out += QStringLiteral("%1 --> %2\n%3\n\n")
               .arg(StampOf(cue.start, QLatin1Char('.')),
                    StampOf(cue.end, QLatin1Char('.')), cue.text);
  }
  return out;
}

}  // namespace snapper
