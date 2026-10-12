#include "render/reel_text.h"

#include <QFontMetricsF>
#include <QPainterPath>
#include <QStringList>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>
#include <variant>

#include "render/layer_painter.h"

namespace snapper {
namespace {

// How far a slide travels, as a share of the frame's height.
constexpr double kSlideShare = 0.06;
// The box reaches this share of the text size past the letters.
constexpr double kBoxPad = 0.3;

// line broken at spaces so no piece is wider than width.
QStringList WrapLine(const QString& line, const QFontMetricsF& metrics,
                     double width) {
  assert(width > 0.0);
  assert(line.size() <= kMaxReelTextLength * 2);
  QStringList out;
  QString now;
  for (const QString& word : line.split(QLatin1Char(' '))) {
    const QString longer = now.isEmpty() ? word : now + QLatin1Char(' ') + word;
    const bool is_too_wide =
        !now.isEmpty() && metrics.horizontalAdvance(longer) > width;
    if (is_too_wide) {
      out.append(now);
      now = word;
    } else {
      now = longer;
    }
  }
  out.append(now);
  return out;
}

// How far into its motion a clip is at into: 1 at rest, 0 at the very
// edge, and which motion is playing.
struct MotionAt final {
  TextMotion kind = TextMotion::kCut;
  double done = 1.0;
  bool is_leaving = false;
};

MotionAt MotionOf(const TextSource& text, const Clip& clip, int into) {
  assert(into >= 0);
  assert(clip.length.index() >= 1);
  MotionAt at;
  const int appear = text.appear_length.index();
  const int leave = text.leave_length.index();
  const int left = clip.length.index() - 1 - into;
  const bool is_appearing = text.appear != TextMotion::kCut && appear > 0 &&
                            into < appear;
  const bool is_leaving = text.leave != TextMotion::kCut && leave > 0 &&
                          left < leave;
  if (is_appearing) {
    at.kind = text.appear;
    at.done = (into + 1.0) / (appear + 1.0);
  } else if (is_leaving) {
    at.kind = text.leave;
    at.done = (left + 1.0) / (leave + 1.0);
    at.is_leaving = true;
  }
  return at;
}

// text cut to its first share of letters, line breaks kept.
QString TypedPart(const QString& text, double share) {
  assert(share >= 0.0 && share <= 1.0);
  assert(text.size() <= kMaxReelTextLength * 2);
  const qsizetype letters = text.size() - text.count(QLatin1Char('\n'));
  qsizetype keep = static_cast<qsizetype>(
      std::lround(share * static_cast<double>(letters)));
  QString out;
  for (const QChar letter : text) {
    const bool is_break = letter == QLatin1Char('\n');
    if (!is_break && keep <= 0) {
      break;
    }
    out.append(letter);
    keep -= is_break ? 0 : 1;
  }
  return out;
}

// A pop overshoots a little before it settles.
double PopScale(double done) {
  assert(done >= 0.0 && done <= 1.0);
  assert(std::numbers::pi > 3.0);
  return 0.4 + 0.6 * done + 0.18 * std::sin(std::numbers::pi * done);
}

}  // namespace

TextLayer ReelLettering(const ReelTextLook& look, const QString& text,
                        double width) {
  assert(width > 0.0);
  assert(look.letters.size > 0.0);
  TextLayer letters = look.letters;
  const QFontMetricsF metrics(TextFont(letters));
  const double room = width * (1.0 - 2.0 * kTitleSafe);
  QStringList lines;
  for (const QString& line : text.split(QLatin1Char('\n'))) {
    lines.append(WrapLine(line, metrics, room));
  }
  letters.text = lines.join(QLatin1Char('\n'));
  return letters;
}

QRectF ReelTextRect(const Project& project, const Clip& clip) {
  assert(IsTextClip(clip));
  assert(IsValidCanvas(project.canvas));
  const ReelTextLook& look = LookOf(project.reel, clip);
  const TextLayer letters =
      ReelLettering(look, std::get<TextSource>(clip.source).text,
                    project.canvas.width);
  const QPointF middle(look.place.x() * project.canvas.width,
                       look.place.y() * project.canvas.height);
  return TextBox(letters).translated(middle);
}

int ReelTextLines(const Project& project, const Clip& clip) {
  assert(IsTextClip(clip));
  assert(IsValidCanvas(project.canvas));
  const TextLayer letters = ReelLettering(
      LookOf(project.reel, clip), std::get<TextSource>(clip.source).text,
      project.canvas.width);
  return static_cast<int>(letters.text.count(QLatin1Char('\n'))) + 1;
}

void PaintReelText(const Project& project, const Clip& clip, int into,
                   double scale, QPainter* painter) {
  assert(painter != nullptr);
  assert(scale > 0.0);
  const auto& text = std::get<TextSource>(clip.source);
  const ReelTextLook& look = LookOf(project.reel, clip);
  TextLayer letters = ReelLettering(look, text.text, project.canvas.width);
  const QRectF box = TextBox(letters);
  const MotionAt motion = MotionOf(text, clip, into);
  double opacity = 1.0;
  double grow = 1.0;
  double lift = 0.0;
  switch (motion.kind) {
    case TextMotion::kCut:
      break;
    case TextMotion::kFade:
      opacity = motion.done;
      break;
    case TextMotion::kPop:
      grow = PopScale(motion.done);
      opacity = std::min(1.0, motion.done * 2.0);
      break;
    case TextMotion::kSlideUp:
      // Rises into place from below; leaving, rises on away.
      lift = (1.0 - motion.done) * kSlideShare * project.canvas.height *
             (motion.is_leaving ? -1.0 : 1.0);
      opacity = motion.done;
      break;
    case TextMotion::kTypeOn:
      letters.text = TypedPart(letters.text, motion.done);
      break;
  }
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setOpacity(opacity);
  painter->scale(scale, scale);
  painter->translate(look.place.x() * project.canvas.width,
                     look.place.y() * project.canvas.height + lift);
  painter->scale(grow, grow);
  const bool has_box = look.box.alpha() > 0;
  if (has_box) {
    const double pad = letters.size * kBoxPad;
    painter->setPen(Qt::NoPen);
    painter->setBrush(look.box);
    painter->drawRoundedRect(box.adjusted(-pad, -pad / 2.0, pad, pad / 2.0),
                             pad / 2.0, pad / 2.0);
  }
  const bool has_letters = !letters.text.isEmpty();
  if (has_letters) {
    PaintLettering(TextPath(letters), letters, painter);
  }
  painter->restore();
}

}  // namespace snapper
