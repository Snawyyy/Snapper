#ifndef SNAPPER_EDIT_CHARACTER_STYLES_H_
#define SNAPPER_EDIT_CHARACTER_STYLES_H_

#include <QString>

#include "base/error.h"
#include "model/character.h"

namespace snapper {

// The edits of a character's named styles (VectorStyle). Pure: each
// changes the character handed in, or says why it can't.

// A new style with look (curves and style id dropped), named name or,
// when that is taken or empty, a fresh "Style 2". Answers its id.
Result<StyleId> AddStyle(Character* character, const QString& name,
                         const VectorShape& look);
Result<void> RenameStyle(Character* character, StyleId id,
                         const QString& name);
// Every shape using the style keeps its look as its own, then the style
// goes.
Result<void> RemoveStyle(Character* character, StyleId id);
// Makes part's shape use style id, or with an invalid id its own look
// again, keeping the look it showed.
Result<void> UseStyle(Character* character, PartId part, int shape,
                      StyleId id);
// The look the Look panel edits: a shape using a style changes the
// style (and so every shape using it); any other shape changes alone.
Result<void> SetShapeLook(Character* character, PartId part, int shape,
                          const VectorShape& look);
Result<void> SetStyleLook(Character* character, StyleId id,
                          const VectorShape& look);

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHARACTER_STYLES_H_
