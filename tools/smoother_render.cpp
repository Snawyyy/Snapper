// Development tool, not a test: runs the Comp tab's Line smoother (the
// OLM Smoother rebuild, render/comp_smoother.h) over a PNG so it can be
// looked at without the app.
//
//   smoother_render sample <out.png>    writes aliased lines and cels
//   smoother_render <in.png> <out.png> [name=value ...] [zoom=4]
//
// Names are the smoother's own ("Smoothness=0", "Gamma Correction=Gamma
// Colors", "Gamma Color 1=#000000"); zoom enlarges the result by whole
// pixels so the steps can be seen.

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QString>

#include <algorithm>
#include <cassert>
#include <cstdio>

#include "fsp/olm_smoother_info.h"
#include "render/comp_smoother.h"

namespace {

using snapper::FspParam;
using snapper::FspParamType;
using snapper::FspValues;

constexpr int kMaxArgs = 64;
constexpr int kMaxZoom = 16;

void DrawLines(QPainter* painter) {
  assert(painter != nullptr);
  assert(!painter->testRenderHint(QPainter::Antialiasing));
  for (int i = 0; i < 6; ++i) {
    painter->setPen(QPen(Qt::black, i < 3 ? 1 : 3));
    painter->drawLine(10 + i * 14, 10, 60 + i * 40, 90 - i * 12);
  }
  painter->setPen(QPen(Qt::black, 2));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(QRect(200, 12, 90, 70));
  painter->drawRect(QRect(310, 20, 50, 40));
}

void DrawCel(QPainter* painter) {
  assert(painter != nullptr);
  assert(!painter->testRenderHint(QPainter::Antialiasing));
  // A skin cel shaded in steps one level apart, as a gradation in a cel.
  for (int band = 0; band < 40; ++band) {
    painter->fillRect(QRect(20, 110 + band * 2, 150, 2),
                      QColor(250 - band / 4, 214 - band / 4, 190 - band / 4));
  }
  painter->setPen(QPen(Qt::black, 2));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(QRect(20, 110, 150, 80));
  painter->setPen(Qt::NoPen);
  painter->setBrush(QColor(220, 30, 40));
  painter->drawEllipse(QRect(200, 110, 70, 70));
  painter->setBrush(QColor(40, 60, 220));
  painter->drawEllipse(QRect(240, 130, 60, 60));
  painter->setBrush(QColor(220, 30, 40));
  painter->drawPolygon(QPolygon({QPoint(310, 190), QPoint(390, 110),
                                 QPoint(390, 190)}));
}

int WriteSample(const QString& path) {
  assert(!path.isEmpty());
  QImage sample(400, 200, QImage::Format_ARGB32_Premultiplied);
  sample.fill(Qt::white);
  QPainter painter(&sample);
  DrawLines(&painter);
  DrawCel(&painter);
  painter.end();
  const bool did_save = sample.save(path);
  assert(sample.width() == 400);
  return did_save ? 0 : 1;
}

const FspParam* ParamNamed(const QString& name) {
  assert(!name.isNull());
  const auto& info = snapper::OlmSmootherInfo();
  assert(!info.params.empty());
  for (const FspParam& param : info.params) {
    const bool is_it = param.name == name;
    if (is_it) {
      return &param;
    }
  }
  return nullptr;
}

bool ReadValue(const FspParam& param, const QString& text,
               FspValues* values) {
  assert(values != nullptr);
  assert(param.slot >= 0);
  const auto slot = static_cast<size_t>(param.slot);
  bool is_number = false;
  const double number = text.toDouble(&is_number);
  const bool is_color = param.type == FspParamType::kColor;
  if (is_color) {
    const QColor color = QColor::fromString(text);
    (*values)[slot] = static_cast<double>(color.rgb() & 0xFFFFFFu);
    return color.isValid();
  }
  const qsizetype index = param.choices.indexOf(text);
  const bool is_choice = index >= 0;
  (*values)[slot] = is_choice ? static_cast<double>(index + 1) : number;
  return is_choice || is_number;
}

QImage Zoomed(const QImage& picture, int zoom) {
  assert(zoom >= 1 && zoom <= kMaxZoom);
  assert(!picture.isNull());
  return picture.scaled(picture.size() * zoom, Qt::IgnoreAspectRatio,
                        Qt::FastTransformation);
}

int Smooth(int argc, char** argv) {
  assert(argc >= 3 && argc <= kMaxArgs);
  assert(argv != nullptr);
  const QImage input = QImage(QString::fromLocal8Bit(argv[1]))
                           .convertToFormat(
                               QImage::Format_ARGB32_Premultiplied);
  const bool has_input = !input.isNull();
  if (!has_input) {
    std::fprintf(stderr, "cannot read %s\n", argv[1]);
    return 1;
  }
  FspValues values = snapper::DefaultFspValues(snapper::OlmSmootherInfo());
  int zoom = 1;
  for (int i = 3; i < argc; ++i) {
    const QString arg = QString::fromLocal8Bit(argv[i]);
    const qsizetype equals = arg.indexOf(QLatin1Char('='));
    const QString name = arg.left(equals);
    const QString text = arg.mid(equals + 1);
    const bool is_zoom = name == QStringLiteral("zoom");
    if (is_zoom) {
      zoom = std::clamp(text.toInt(), 1, kMaxZoom);
      continue;
    }
    const FspParam* param = ParamNamed(name);
    const bool is_ok =
        equals > 0 && param != nullptr && ReadValue(*param, text, &values);
    if (!is_ok) {
      std::fprintf(stderr, "bad argument: %s\n", argv[i]);
      return 1;
    }
  }
  const snapper::SmootherParams params = snapper::SmootherParamsOf(values);
  const QImage output = snapper::SmoothCel(input, params);
  const bool did_save =
      Zoomed(output, zoom).save(QString::fromLocal8Bit(argv[2]));
  return did_save ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc >= 1);
  assert(argv != nullptr);
  const bool is_usable = argc >= 3 && argc <= kMaxArgs;
  if (!is_usable) {
    std::fprintf(stderr,
                 "usage: smoother_render sample <out.png>\n"
                 "       smoother_render <in.png> <out.png> [name=value "
                 "...] [zoom=n]\n");
    return 2;
  }
  const bool wants_sample = QString::fromLocal8Bit(argv[1]) == "sample";
  return wants_sample ? WriteSample(QString::fromLocal8Bit(argv[2]))
                      : Smooth(argc, argv);
}
