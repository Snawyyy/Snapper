#ifndef SNAPPER_UI_PLATED_BOX_H_
#define SNAPPER_UI_PLATED_BOX_H_

#include <QLabel>
#include <QLayout>
#include <QString>
#include <QWidget>

namespace snapper {

// Makes box a plated section of its own (panel_kit.h PlatedSection): a
// title plate at the edge over a sunken frame whose body is laid out by
// body (which box keeps owning, so it may be a member). For panels'
// boxes that show and hide themselves, like the rig's Look, Skins and
// Collider. box must have no layout yet. The title plate.
QLabel* PlateBox(QWidget* box, const QString& title, QLayout* body);

}  // namespace snapper

#endif  // SNAPPER_UI_PLATED_BOX_H_
