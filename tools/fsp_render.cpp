// Development tool, not a test: runs one F's Plugins effect over a PNG so
// a port can be looked at without the app.
//
//   fsp_render                      lists the plugins and their parameters
//   fsp_render <plugin> <in.png> <out.png> [name=value ...] [scale=0.5]
//
// Values not given keep the plugin's default. A point is x,y in percent
// of the picture, a color #RRGGBB, a popup its number or its entry.

#include <QColor>
#include <QImage>
#include <QString>
#include <QStringList>

#include <cassert>
#include <cstdio>

#include "fsp/fsp_effects.h"

namespace {

using snapper::FspEffectInfo;
using snapper::FspParam;
using snapper::FspParamType;
using snapper::FspValues;

constexpr int kMaxArgs = 256;

const char* TypeName(FspParamType type) {
  assert(type >= FspParamType::kSlider);
  assert(type <= FspParamType::kPoint);
  switch (type) {
    case FspParamType::kSlider:
      return "slider";
    case FspParamType::kFloat:
      return "float";
    case FspParamType::kAngle:
      return "angle";
    case FspParamType::kCheckbox:
      return "checkbox";
    case FspParamType::kPopup:
      return "popup";
    case FspParamType::kColor:
      return "color";
    case FspParamType::kPoint:
      return "point";
  }
  return "?";
}

void PrintParam(const FspParam& param) {
  assert(!param.name.isNull());
  assert(param.slot >= 0);
  const QString choices = param.choices.join(QStringLiteral("|"));
  std::printf("    %-28s %-8s %g..%g (slider %g..%g) default %g",
              qPrintable(param.name), TypeName(param.type), param.low,
              param.high, param.slider_low, param.slider_high,
              param.neutral);
  const bool is_point = param.type == FspParamType::kPoint;
  if (is_point) {
    std::printf(",%g", param.neutral_y);
  }
  const bool has_choices = !choices.isEmpty();
  if (has_choices) {
    std::printf(" [%s]", qPrintable(choices));
  }
  std::printf("%s\n", param.is_hidden ? " (hidden)" : "");
}

int ListPlugins() {
  const std::vector<FspEffectInfo>& all = snapper::AllFspEffects();
  assert(all.size() < 4096);
  assert(all.empty() || !all.front().id.isEmpty());
  std::printf("%zu plugin(s)\n", all.size());
  for (const FspEffectInfo& info : all) {
    std::printf("%s (%s), %d value slot(s)\n", qPrintable(info.id),
                qPrintable(info.name), info.slot_count);
    for (const FspParam& param : info.params) {
      PrintParam(param);
    }
  }
  return 0;
}

// The value of one parameter as typed; false when it does not read.
bool ParseValue(const FspParam& param, const QString& text,
                FspValues* values) {
  assert(values != nullptr);
  assert(param.slot >= 0);
  const std::size_t slot = static_cast<std::size_t>(param.slot);
  bool is_number = false;
  const double number = text.toDouble(&is_number);
  switch (param.type) {
    case FspParamType::kPoint: {
      const QStringList parts = text.split(QLatin1Char(','));
      const bool is_pair = parts.size() == 2;
      if (!is_pair) {
        return false;
      }
      (*values)[slot] = parts[0].toDouble(&is_number);
      (*values)[slot + 1] = parts[1].toDouble(&is_number);
      return is_number;
    }
    case FspParamType::kColor: {
      const QColor color = QColor::fromString(text);
      const bool is_color = color.isValid();
      if (is_color) {
        (*values)[slot] = static_cast<double>(color.rgb() & 0xFFFFFFu);
      }
      return is_color || is_number;
    }
    case FspParamType::kPopup: {
      const qsizetype index = param.choices.indexOf(text);
      const bool is_choice = index >= 0;
      (*values)[slot] = is_choice ? static_cast<double>(index + 1) : number;
      return is_choice || is_number;
    }
    default:
      (*values)[slot] = number;
      return is_number;
  }
}

const FspParam* ParamNamed(const FspEffectInfo& info, const QString& name) {
  assert(!name.isNull());
  assert(info.slot_count >= 0);
  for (const FspParam& param : info.params) {
    const bool is_it = param.name == name;
    if (is_it) {
      return &param;
    }
  }
  return nullptr;
}

// Reads name=value arguments into values and scale; false on a bad one.
bool ReadArgs(const FspEffectInfo& info, int argc, char** argv,
              FspValues* values, double* scale) {
  assert(values != nullptr && scale != nullptr);
  assert(argc <= kMaxArgs);
  for (int i = 4; i < argc; ++i) {
    const QString arg = QString::fromLocal8Bit(argv[i]);
    const qsizetype equals = arg.indexOf(QLatin1Char('='));
    const QString name = arg.left(equals);
    const QString text = arg.mid(equals + 1);
    const bool is_scale = name == QStringLiteral("scale");
    if (is_scale) {
      *scale = text.toDouble();
      continue;
    }
    const FspParam* param = ParamNamed(info, name);
    const bool is_ok = equals > 0 && param != nullptr &&
                       ParseValue(*param, text, values);
    if (!is_ok) {
      std::fprintf(stderr, "bad argument: %s\n", argv[i]);
      return false;
    }
  }
  return *scale > 0.0;
}

int Render(int argc, char** argv) {
  assert(argc >= 4);
  assert(argc <= kMaxArgs);
  const FspEffectInfo* info =
      snapper::FindFspEffect(QString::fromLocal8Bit(argv[1]));
  const bool has_plugin = info != nullptr;
  if (!has_plugin) {
    std::fprintf(stderr, "no plugin named %s\n", argv[1]);
    return 1;
  }
  const QImage input(QString::fromLocal8Bit(argv[2]));
  const bool has_input = !input.isNull();
  if (!has_input) {
    std::fprintf(stderr, "cannot read %s\n", argv[2]);
    return 1;
  }
  FspValues values = snapper::DefaultFspValues(*info);
  double scale = 1.0;
  const bool has_args = ReadArgs(*info, argc, argv, &values, &scale);
  if (!has_args) {
    return 1;
  }
  const QImage output =
      snapper::RunFspEffect(*info, values, input, scale);
  const bool did_render = !output.isNull();
  if (!did_render) {
    std::fprintf(stderr, "%s did not render\n", argv[1]);
    return 1;
  }
  const bool did_save = output.save(QString::fromLocal8Bit(argv[3]));
  if (!did_save) {
    std::fprintf(stderr, "cannot write %s\n", argv[3]);
    return 1;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc >= 1);
  assert(argv != nullptr);
  const bool wants_list = argc == 1;
  if (wants_list) {
    return ListPlugins();
  }
  const bool is_usable = argc >= 4 && argc <= kMaxArgs;
  if (!is_usable) {
    std::fprintf(stderr,
                 "usage: fsp_render [<plugin> <in.png> <out.png> "
                 "[name=value ...] [scale=s]]\n");
    return 2;
  }
  return Render(argc, argv);
}
