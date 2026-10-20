#include "edit/character_styles.h"

#include <algorithm>
#include <cassert>

#include "base/text.h"

namespace snapper {
namespace {

Error NoStyle() { return Error{Tr("That style is gone.")}; }

// look with only what a style keeps.
VectorShape BareLook(const VectorShape& look) {
  assert(look.stroke_width >= 0.0);
  assert(static_cast<int>(look.brush) < kLineBrushCount);
  VectorShape bare = look;
  bare.curves.clear();
  bare.style = StyleId();
  bare.stroke_width = std::clamp(bare.stroke_width, 0.0,
                                 static_cast<double>(kMaxStrokeWidth));
  return bare;
}

bool IsNameTaken(const Character& character, const QString& name) {
  assert(character.styles.size() <= static_cast<size_t>(kMaxCharacterStyles));
  return std::ranges::any_of(character.styles,
                             [&name](const VectorStyle& style) {
                               return style.name == name;
                             });
}

Result<VectorShape*> ShapeIn(Character* character, PartId part, int shape) {
  assert(character != nullptr);
  assert(part.value() >= 0);
  CharacterPart* found = FindPart(character, part);
  const bool is_there = found != nullptr && found->kind == PartKind::kVector &&
                        shape >= 0 &&
                        static_cast<size_t>(shape) < found->drawing.shapes.size();
  if (!is_there) {
    return std::unexpected(Error{Tr("Pick a shape first.")});
  }
  return &found->drawing.shapes[static_cast<size_t>(shape)];
}

}  // namespace

Result<StyleId> AddStyle(Character* character, const QString& name,
                         const VectorShape& look) {
  assert(character != nullptr);
  assert(name.size() < 100000);
  if (character->styles.size() >= static_cast<size_t>(kMaxCharacterStyles)) {
    return std::unexpected(
        Error{Tr("A character holds at most %1 styles.").arg(kMaxCharacterStyles)});
  }
  QString fresh = name.trimmed();
  for (int n = static_cast<int>(character->styles.size()) + 1;
       fresh.isEmpty() || IsNameTaken(*character, fresh); ++n) {
    fresh = Tr("Style %1").arg(n);
  }
  const StyleId id(character->next_id++);
  character->styles.push_back({id, fresh, BareLook(look)});
  return id;
}

Result<void> RenameStyle(Character* character, StyleId id,
                         const QString& name) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  VectorStyle* style = FindStyle(character, id);
  if (style == nullptr) {
    return std::unexpected(NoStyle());
  }
  const QString trimmed = name.trimmed();
  if (trimmed.isEmpty()) {
    return std::unexpected(Error{Tr("A style needs a name.")});
  }
  if (trimmed != style->name && IsNameTaken(*character, trimmed)) {
    return std::unexpected(
        Error{Tr("There is a style called %1 already.").arg(trimmed)});
  }
  style->name = trimmed;
  return {};
}

Result<void> RemoveStyle(Character* character, StyleId id) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  const VectorStyle* style = FindStyle(character, id);
  if (style == nullptr) {
    return std::unexpected(NoStyle());
  }
  for (CharacterPart& part : character->parts) {
    for (VectorShape& shape : part.drawing.shapes) {
      if (shape.style == id) {
        shape = LookOf(*character, shape);
        shape.style = StyleId();
      }
    }
  }
  std::erase_if(character->styles,
                [id](const VectorStyle& kept) { return kept.id == id; });
  return {};
}

Result<void> UseStyle(Character* character, PartId part, int shape,
                      StyleId id) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  const Result<VectorShape*> found = ShapeIn(character, part, shape);
  if (!found) {
    return std::unexpected(found.error());
  }
  if (id.IsValid() && FindStyle(*character, id) == nullptr) {
    return std::unexpected(NoStyle());
  }
  VectorShape& changed = **found;
  changed = LookOf(*character, changed);
  changed.style = id;
  return {};
}

Result<void> SetShapeLook(Character* character, PartId part, int shape,
                          const VectorShape& look) {
  assert(character != nullptr);
  assert(look.stroke_width >= 0.0);
  const Result<VectorShape*> found = ShapeIn(character, part, shape);
  if (!found) {
    return std::unexpected(found.error());
  }
  const StyleId style = (*found)->style;
  if (style.IsValid() && FindStyle(*character, style) != nullptr) {
    return SetStyleLook(character, style, look);
  }
  VectorShape changed = BareLook(look);
  changed.curves = (*found)->curves;
  **found = std::move(changed);
  return {};
}

Result<void> SetStyleLook(Character* character, StyleId id,
                          const VectorShape& look) {
  assert(character != nullptr);
  assert(id.value() >= 0);
  VectorStyle* style = FindStyle(character, id);
  if (style == nullptr) {
    return std::unexpected(NoStyle());
  }
  style->look = BareLook(look);
  return {};
}

}  // namespace snapper
