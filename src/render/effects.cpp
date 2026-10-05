#include "render/effects.h"

#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/jitter.h"

namespace snapper {
namespace {

// Halftone dots and glitch tears at full canvas size, in pixels.
constexpr double kHalftoneCell = 10.0;
constexpr double kGlitchShift = 24.0;
constexpr int kGlitchTears = 12;

// Draws over with what painted itself onto a copy, at strength amount.
void MixIn(const QImage& changed, double amount, QImage* frame) {
  assert(frame != nullptr);
  assert(amount >= 0.0 && amount <= 1.0);
  QPainter painter(frame);
  painter.setOpacity(amount);
  painter.drawImage(0, 0, changed);
}

void Invert(double amount, QImage* frame) {
  assert(frame != nullptr);
  assert(amount >= 0.0);
  QImage inverted = frame->copy();
  inverted.invertPixels();
  MixIn(inverted, amount, frame);
}

void ZoomPunch(double amount, QImage* frame) {
  assert(frame != nullptr);
  assert(amount >= 0.0);
  const QImage before = frame->copy();
  const double zoom = 1.0 + 0.5 * amount;
  QPainter painter(frame);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.translate(frame->width() / 2.0, frame->height() / 2.0);
  painter.scale(zoom, zoom);
  painter.translate(-frame->width() / 2.0, -frame->height() / 2.0);
  painter.drawImage(0, 0, before);
}

void Halftone(double amount, double scale, QImage* frame) {
  assert(frame != nullptr);
  assert(scale > 0.0);
  const double cell = std::max(2.0, kHalftoneCell * scale);
  QImage dots(frame->size(), QImage::Format_ARGB32_Premultiplied);
  dots.fill(Qt::white);
  QPainter painter(&dots);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  const int columns = static_cast<int>(frame->width() / cell) + 1;
  const int rows = static_cast<int>(frame->height() / cell) + 1;
  for (int row = 0; row < rows; ++row) {
    for (int column = 0; column < columns; ++column) {
      const QPointF center((column + 0.5) * cell, (row + 0.5) * cell);
      const QRgb pixel = frame->pixel(
          std::min(static_cast<int>(center.x()), frame->width() - 1),
          std::min(static_cast<int>(center.y()), frame->height() - 1));
      const double dark = 1.0 - qGray(pixel) / 255.0;
      painter.setBrush(QColor(pixel).darker(140));
      const double radius = cell * 0.7 * std::sqrt(dark);
      painter.drawEllipse(center, radius, radius);
    }
  }
  painter.end();
  MixIn(dots, amount, frame);
}

// Red read from the right, blue from the left: the colours split.
QImage SplitChannels(const QImage& before, int shift) {
  assert(before.format() == QImage::Format_ARGB32_Premultiplied);
  assert(shift >= 0);
  QImage split(before.size(), before.format());
  const int last = before.width() - 1;
  for (int y = 0; y < before.height(); ++y) {
    const auto* in = reinterpret_cast<const QRgb*>(before.constScanLine(y));
    auto* out = reinterpret_cast<QRgb*>(split.scanLine(y));
    for (int x = 0; x <= last; ++x) {
      const QRgb right = in[std::min(x + shift, last)];
      const QRgb left = in[std::max(x - shift, 0)];
      const int alpha =
          std::max({qAlpha(right), qAlpha(in[x]), qAlpha(left)});
      out[x] = qRgba(std::min(qRed(right), alpha),
                     std::min(qGreen(in[x]), alpha),
                     std::min(qBlue(left), alpha), alpha);
    }
  }
  return split;
}

void Glitch(double amount, double scale, int seed, QImage* frame) {
  assert(frame != nullptr);
  assert(seed >= 0);
  const QImage before =
      frame->convertToFormat(QImage::Format_ARGB32_Premultiplied);
  const double shift = kGlitchShift * scale * amount;
  const QImage split =
      SplitChannels(before, static_cast<int>(std::lround(shift)));
  QPainter painter(frame);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.drawImage(0, 0, split);
  // Torn bands slid sideways.
  for (int tear = 0; tear < kGlitchTears; ++tear) {
    const double top = (Jitter(seed, tear) + 1.0) / 2.0 * frame->height();
    const double height =
        (Jitter(seed, tear + kGlitchTears) + 1.0) * 0.02 * frame->height() +
        1.0;
    const double slide = Jitter(seed, tear + 2 * kGlitchTears) * shift * 3.0;
    const QRectF band(0, top, frame->width(), height);
    painter.drawImage(band.translated(slide, 0), split, band);
  }
}

void Posterize(double amount, QImage* frame) {
  assert(frame != nullptr);
  assert(amount >= 0.0);
  const int levels = static_cast<int>(std::lround(256.0 - 252.0 * amount));
  const int step = 256 / std::max(levels, 2);
  QImage poster = frame->convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < poster.height(); ++y) {
    auto* row = reinterpret_cast<QRgb*>(poster.scanLine(y));
    for (int x = 0; x < poster.width(); ++x) {
      const auto snap = [step](int value) {
        return std::min(255, value / step * step + step / 2);
      };
      row[x] = qRgba(snap(qRed(row[x])), snap(qGreen(row[x])),
                     snap(qBlue(row[x])), qAlpha(row[x]));
    }
  }
  *frame = poster.convertToFormat(frame->format());
}

}  // namespace

void ApplyEffect(EffectKind kind, double amount, QColor color, double scale,
                 int seed, QImage* frame) {
  assert(frame != nullptr && !frame->isNull());
  assert(scale > 0.0);
  const double strength = std::clamp(amount, 0.0, 1.0);
  const bool is_off = strength <= 0.0;
  if (is_off) {
    return;
  }
  switch (kind) {
    case EffectKind::kFlash:
    case EffectKind::kFill: {
      // A flash lightens; a fill covers.
      QPainter painter(frame);
      painter.setOpacity(strength);
      painter.setCompositionMode(kind == EffectKind::kFlash
                                     ? QPainter::CompositionMode_Screen
                                     : QPainter::CompositionMode_SourceOver);
      painter.fillRect(frame->rect(), color);
      break;
    }
    case EffectKind::kInvert:
      Invert(strength, frame);
      break;
    case EffectKind::kZoomPunch:
      ZoomPunch(strength, frame);
      break;
    case EffectKind::kHalftone:
      Halftone(strength, scale, frame);
      break;
    case EffectKind::kGlitch:
      Glitch(strength, scale, seed, frame);
      break;
    case EffectKind::kPosterize:
      Posterize(strength, frame);
      break;
  }
}

}  // namespace snapper
