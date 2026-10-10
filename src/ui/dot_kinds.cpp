#include "ui/dot_kinds.h"

#include <QCoreApplication>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>

namespace snapper {
namespace {

QString Tr(const char* text) {
  assert(text != nullptr);
  assert(text[0] != '\0');
  return QCoreApplication::translate("DotKinds", text);
}

}  // namespace

bool RowAppliesTo(DragField field, Material material) {
  assert(static_cast<int>(material) >= 0);
  assert(static_cast<int>(field) >= 0);
  switch (material) {
    case Material::kStill:
      return false;
    case Material::kDrag:
      return field == DragField::kLag || field == DragField::kBounce;
    case Material::kCloth:
    case Material::kJelly:
      return field != DragField::kLag && field != DragField::kBounce;
  }
  return false;
}

double FieldValue(const DragNode& node, DragField field) {
  assert(static_cast<int>(field) >= 0);
  assert(node.point >= 0);
  switch (field) {
    case DragField::kLag:
      return node.lag;
    case DragField::kBounce:
      return node.bounce;
    case DragField::kStretch:
      return node.stretch;
    case DragField::kInertia:
      return node.inertia;
    case DragField::kAir:
      return node.air;
    case DragField::kGravity:
      return node.gravity;
    case DragField::kSpring:
      return node.spring;
  }
  return 0.0;
}

Material MaterialOf(const DragNode* node) {
  assert(kMaterialCount == 4);
  const bool is_still = node == nullptr;
  if (is_still) {
    return Material::kStill;
  }
  assert(static_cast<int>(node->kind) < kNodeKindCount);
  switch (node->kind) {
    case NodeKind::kDrag:
      return Material::kDrag;
    case NodeKind::kCloth:
      return Material::kCloth;
    case NodeKind::kJelly:
      return Material::kJelly;
  }
  return Material::kStill;
}

NodeKind KindOf(Material material) {
  assert(static_cast<int>(material) < kMaterialCount);
  assert(static_cast<int>(material) >= 0);
  switch (material) {
    case Material::kCloth:
      return NodeKind::kCloth;
    case Material::kJelly:
      return NodeKind::kJelly;
    case Material::kStill:
    case Material::kDrag:
      return NodeKind::kDrag;
  }
  return NodeKind::kDrag;
}

QString MaterialName(Material material) {
  assert(static_cast<int>(material) < kMaterialCount);
  assert(static_cast<int>(material) >= 0);
  switch (material) {
    case Material::kStill:
      return Tr("Still");
    case Material::kDrag:
      return Tr("Drag");
    case Material::kCloth:
      return Tr("Cloth");
    case Material::kJelly:
      return Tr("Jelly");
  }
  return QString();
}

KindMix::KindMix(const std::vector<const DragNode*>& per_dot)
    : total_(static_cast<int>(per_dot.size())) {
  assert(per_dot.size() < 10000000);
  for (const DragNode* node : per_dot) {
    ++counts_[static_cast<size_t>(MaterialOf(node))];
  }
  assert(std::count(counts_.begin(), counts_.end(), 0) <= kMaterialCount);
}

int KindMix::CountOf(Material material) const {
  assert(static_cast<int>(material) < kMaterialCount);
  assert(static_cast<int>(material) >= 0);
  return counts_[static_cast<size_t>(material)];
}

Share KindMix::ShareOf(Material material) const {
  const int count = CountOf(material);
  assert(total_ >= 0);
  const bool is_none = count == 0;
  const bool is_all = count == total_;
  assert(count <= total_);
  return is_none ? Share::kNone : is_all ? Share::kAll : Share::kSome;
}

bool KindMix::IsMixed() const {
  assert(total_ >= 0);
  const auto used = std::count_if(counts_.begin(), counts_.end(),
                                  [](int n) { return n > 0; });
  assert(used <= kMaterialCount);
  return used > 1;
}

Material KindMix::Chosen() const {
  assert(kMaterialCount > 0);
  const bool is_single = total_ > 0 && !IsMixed();
  for (int i = 0; i < kMaterialCount && is_single; ++i) {
    const bool is_used = counts_[static_cast<size_t>(i)] > 0;
    if (is_used) {
      return static_cast<Material>(i);
    }
  }
  assert(total_ >= 0);
  return Material::kStill;
}

std::vector<DragField> FineRowsOf(const KindMix& mix) {
  assert(mix.total() >= 0);
  constexpr DragField kOrder[] = {
      DragField::kLag,     DragField::kBounce,  DragField::kStretch,
      DragField::kSpring,  DragField::kInertia, DragField::kAir,
      DragField::kGravity};
  std::vector<DragField> rows;
  for (const DragField field : kOrder) {
    for (int i = 0; i < kMaterialCount; ++i) {
      const auto material = static_cast<Material>(i);
      const bool is_used = mix.Uses(material) && RowAppliesTo(field, material);
      if (is_used) {
        rows.push_back(field);
        break;
      }
    }
  }
  assert(rows.size() <= 7);
  return rows;
}

QString FineLabel(DragField field, const KindMix& mix) {
  assert(mix.total() >= 0);
  assert(static_cast<int>(field) >= 0);
  const bool is_squish =
      field == DragField::kStretch && !mix.Uses(Material::kCloth);
  switch (field) {
    case DragField::kLag:
      return Tr("Lag");
    case DragField::kBounce:
    case DragField::kSpring:
      return Tr("Bounce");
    case DragField::kStretch:
      return is_squish ? Tr("Squish") : Tr("Stretch");
    case DragField::kInertia:
      return Tr("Inertia");
    case DragField::kAir:
      return Tr("Air resistance");
    case DragField::kGravity:
      return Tr("Gravity");
  }
  return QString();
}

QString FineHelp(DragField field, const KindMix& mix) {
  assert(mix.total() >= 0);
  assert(static_cast<int>(field) >= 0);
  const bool is_squish =
      field == DragField::kStretch && !mix.Uses(Material::kCloth);
  switch (field) {
    case DragField::kLag:
      return Tr("How far it trails behind: 0 keeps up, 1 drags far.");
    case DragField::kBounce:
      return Tr("How much it wobbles before settling: 0 settles softly, "
                "1 keeps bouncing.");
    case DragField::kStretch:
      return is_squish
                 ? Tr("How much the blob squashes and stretches as it "
                      "wobbles: 0 stays its drawn shape, 1 squishes "
                      "freely. Its area stays.")
                 : Tr("How far it may pull away from the dots around it: "
                      "0 keeps its drawn distance (a lone dot cannot "
                      "move), 1 a cell further.");
    case DragField::kInertia:
      return Tr("How much it keeps moving when the doll speeds up, stops "
                "or turns: 0 moves with the doll, 1 is heavy and swings "
                "through.");
    case DragField::kAir:
      return Tr("How fast its swinging dies down: 0 rings on longest, 1 "
                "settles soon.");
    case DragField::kGravity:
      return Tr("How hard it hangs down when the doll tilts or turns "
                "over: 0 not at all, 1 strongly. Upright it stays as "
                "drawn.");
    case DragField::kSpring:
      return Tr("How firmly it springs back to its drawn shape: 0 not at "
                "all (cloth just hangs), 1 a firm jiggle.");
  }
  return QString();
}

QString TitleOf(const std::vector<QString>& pieces, const KindMix& mix) {
  assert(pieces.size() == static_cast<size_t>(mix.total()));
  assert(pieces.size() < 10000000);
  const bool is_none = pieces.empty();
  if (is_none) {
    return Tr("Warp dots");
  }
  const std::set<QString> names(pieces.begin(), pieces.end());
  const QString where =
      names.size() == 1
          ? *names.begin()
          : QCoreApplication::translate("DotKinds", "%1 pieces")
                .arg(names.size());
  const bool is_one = pieces.size() == 1;
  QString title = (is_one ? Tr("Warp dot: ") : Tr("Warp dots: ")) + where;
  if (!is_one) {
    title += QCoreApplication::translate("DotKinds", ", %1 picked")
                 .arg(pieces.size());
  }
  return mix.IsMixed() ? title + Tr(", Mixed") : title;
}

bool SameFeel(const Feel& a, const Feel& b) {
  assert(std::isfinite(a.firm + a.heavy));
  assert(std::isfinite(b.firm + b.heavy));
  constexpr double kSame = 0.02;
  return std::abs(a.firm - b.firm) < kSame &&
         std::abs(a.heavy - b.heavy) < kSame;
}

std::vector<Feel> DistinctFeels(const std::vector<const DragNode*>& nodes) {
  assert(nodes.size() < 10000000);
  assert(kMaxPucks > 0);
  std::vector<Feel> feels;
  for (const DragNode* node : nodes) {
    const Feel feel = FeelOf(*node);
    const bool is_known = std::any_of(
        feels.begin(), feels.end(),
        [&feel](const Feel& f) { return SameFeel(f, feel); });
    const bool is_room = feels.size() < kMaxPucks;
    const bool is_new = !is_known && is_room;
    if (is_new) {
      feels.push_back(feel);
    }
  }
  return feels;
}

}  // namespace snapper
