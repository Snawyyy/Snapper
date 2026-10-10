#include "anim/node_feel.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "base/text.h"

namespace snapper {
namespace {

// Each setting runs from its value at feel 0 to its value at feel 1.
struct Span final {
  double at_zero = 0.0;
  double at_one = 0.0;
};

// The spans of one kind: the setting firm sets, the one heavy sets,
// and the ones that follow along.
struct KindSpans final {
  Span lag;
  Span bounce;
  Span stretch;
  Span spring;
  Span inertia;
  Span air;
  Span gravity;
};

KindSpans SpansOf(NodeKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kNodeKindCount);
  switch (kind) {
    case NodeKind::kDrag:
      return {{0.8, 0.05}, {0.1, 0.85}, {}, {}, {}, {}, {}};
    case NodeKind::kCloth:
      return {{}, {}, {0.6, 0.0}, {0.0, 0.6}, {0.3, 0.9}, {0.6, 0.15},
              {0.2, 0.8}};
    case NodeKind::kJelly:
      return {{}, {}, {0.9, 0.2}, {0.25, 0.9}, {0.4, 0.9}, {0.5, 0.15},
              {0.1, 0.5}};
  }
  return {};
}

double At(const Span& span, double t) {
  assert(std::isfinite(t));
  assert(std::isfinite(span.at_zero + span.at_one));
  const double clamped = std::clamp(t, 0.0, 1.0);
  return span.at_zero + (span.at_one - span.at_zero) * clamped;
}

// Where value sits along span, 0 to 1.
double Along(const Span& span, double value) {
  assert(std::isfinite(value));
  assert(std::isfinite(span.at_zero + span.at_one));
  const double width = span.at_one - span.at_zero;
  const bool is_flat = std::abs(width) < 1e-9;
  return is_flat ? 0.5
                 : std::clamp((value - span.at_zero) / width, 0.0, 1.0);
}

}  // namespace

DragNode WithFeel(DragNode node, const Feel& feel) {
  assert(std::isfinite(feel.firm) && std::isfinite(feel.heavy));
  assert(static_cast<int>(node.kind) < kNodeKindCount);
  const KindSpans spans = SpansOf(node.kind);
  const bool is_drag = node.kind == NodeKind::kDrag;
  if (is_drag) {
    node.lag = At(spans.lag, feel.firm);
    node.bounce = At(spans.bounce, feel.heavy);
    return node;
  }
  node.stretch = At(spans.stretch, feel.firm);
  node.spring = At(spans.spring, feel.firm);
  node.inertia = At(spans.inertia, feel.heavy);
  node.air = At(spans.air, feel.heavy);
  node.gravity = At(spans.gravity, feel.heavy);
  return node;
}

Feel FeelOf(const DragNode& node) {
  assert(static_cast<int>(node.kind) >= 0);
  assert(static_cast<int>(node.kind) < kNodeKindCount);
  const KindSpans spans = SpansOf(node.kind);
  const bool is_drag = node.kind == NodeKind::kDrag;
  return is_drag ? Feel{Along(spans.lag, node.lag),
                        Along(spans.bounce, node.bounce)}
                 : Feel{Along(spans.spring, node.spring),
                        Along(spans.inertia, node.inertia)};
}

std::vector<FeelPreset> FeelPresets(NodeKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kNodeKindCount);
  switch (kind) {
    case NodeKind::kDrag:
      return {{Tr("Snappy"), {0.9, 0.2}},
              {Tr("Floaty"), {0.15, 0.5}},
              {Tr("Springy"), {0.5, 0.9}}};
    case NodeKind::kCloth:
      return {{Tr("Silk"), {0.2, 0.3}},
              {Tr("Denim"), {0.8, 0.6}},
              {Tr("Cape"), {0.4, 0.9}}};
    case NodeKind::kJelly:
      return {{Tr("Pudding"), {0.3, 0.5}},
              {Tr("Water balloon"), {0.1, 0.9}},
              {Tr("Firm"), {0.85, 0.4}}};
  }
  return {};
}

}  // namespace snapper
