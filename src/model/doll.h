#ifndef SNAPPER_MODEL_DOLL_H_
#define SNAPPER_MODEL_DOLL_H_

#include <QColor>
#include <QPointF>
#include <QSize>
#include <QString>

#include <map>
#include <memory>
#include <vector>

#include "base/id.h"
#include "model/dot_group.h"
#include "model/smart_dial.h"
#include "model/warp_mesh.h"

namespace snapper {

struct LayerTag;
using LayerId = Id<LayerTag>;

constexpr int kMaxDollPieces = 128;
constexpr int kMaxPieceDrawings = 64;
constexpr int kMaxIkChains = 32;
// Hand-added palette colors a doll keeps.
constexpr int kMaxAddedColors = 64;
constexpr int kMaxWarpCells = 8;
// The most points a warp has: a shape mesh's, which is more than the
// biggest grid's.
constexpr int kMaxWarpPoints = kMaxMeshPoints;
static_assert(kMaxWarpPoints >= (kMaxWarpCells + 1) * (kMaxWarpCells + 1));

// One piece as Krita drew it. Doll space has its origin at the middle
// of the Krita canvas, y down, in pixels.
struct ArtPiece final {
  QString name;
  // PNG names inside the doll folder. All are the same size.
  std::vector<QString> drawings;
  int default_drawing = 0;
  // Top-left corner of the drawings in doll space, and their size.
  QPointF position;
  QSize size;
  // Each drawing's Krita layer name, one per drawing; empty for dolls
  // exported before names were kept (see DrawingName).
  std::vector<QString> layer_names;
  // The Krita groups holding the piece, outermost first.
  std::vector<QString> folder;

  bool operator==(const ArtPiece&) const = default;
};

// The name of drawing index of piece: its Krita layer name, or for old
// dolls "head 1", "head 2" by position.
QString DrawingName(const ArtPiece& piece, int index);

// What Krita exported: art.json. Snapper only reads it.
struct DollArt final {
  QSize canvas;
  // Bottom to top, as Krita stacks them.
  std::vector<ArtPiece> pieces;

  bool operator==(const DollArt&) const = default;
};

// How a piece's drawing bends. With a mesh it is a shape mesh that
// hugs the drawing (see WarpMesh) and columns and rows are 0. Without
// one, 0 by 0 is off, and otherwise the drawing is cut into columns by
// rows cells whose corners can be pushed (the older grid, kept so old
// files bend as they always did).
struct WarpGrid final {
  int columns = 0;
  int rows = 0;
  std::shared_ptr<const WarpMesh> mesh{};

  bool IsMesh() const { return mesh != nullptr; }
  bool IsOn() const { return IsMesh() || (columns > 0 && rows > 0); }
  int PointCount() const {
    return IsMesh() ? static_cast<int>(mesh->points.size())
           : IsOn() ? (columns + 1) * (rows + 1)
                    : 0;
  }
  bool operator==(const WarpGrid& other) const {
    const bool is_same_mesh =
        mesh == other.mesh ||
        (mesh != nullptr && other.mesh != nullptr && *mesh == *other.mesh);
    return columns == other.columns && rows == other.rows && is_same_mesh;
  }
};

// How a drag node moves.
// - kDrag: a lone spring that trails behind and bounces back.
// - kCloth: secondary motion for cloth and hair: cloth nodes next to
//   each other hang together like a sheet from the points around them.
// - kJelly: soft flesh, a chest: the jelly nodes of a piece wobble as
//   one blob that keeps its area, so a squeeze one way makes it bulge
//   the other.
enum class NodeKind { kDrag, kCloth, kJelly };
inline constexpr int kNodeKindCount = 3;

// A warp grid point that moves by itself when the doll moves. A drag
// node uses lag (how far it trails) and bounce (how much it wobbles
// before settling), both 0 to 1. Cloth and jelly use the settings
// below instead, each 0 (none of it) to 1 (a lot). Jelly reads four of
// them as knobs spread evenly by feel (model/node_physics.h): spring is
// Firmness, inertia is Wobble, stretch is Squish, gravity is Droop; it
// has no air.
struct DragNode final {
  int point = 0;
  double lag = 0.5;
  double bounce = 0.5;
  NodeKind kind = NodeKind::kDrag;
  // Cloth: how far it may get from the points around it: 0 keeps its
  // drawn distance, 1 a whole cell further (rubbery). Jelly (Squish):
  // how far the blob squashes and stretches as it wobbles, 0 stays its
  // drawn shape, 1 squishes freely; its area stays.
  double stretch = 0.3;
  // How much of its own motion it keeps when the doll speeds up, stops
  // or turns: 0 moves with the doll, 1 is heavy and swings through.
  // Jelly (Wobble): how many jiggles before it settles, 0 none to 1 six.
  double inertia = 0.7;
  // How fast its motion dies down: 0 rings on longest, 1 settles soon.
  double air = 0.3;
  // How hard it hangs down when the doll tilts or turns over (as drawn
  // it already hangs): 0 not at all, 1 strongly. Jelly (Droop): how far
  // it sags, the same at any firmness.
  double gravity = 0.5;
  // A spring back to its drawn shape: 0 none (cloth just hangs), 1 a
  // firm jiggle. Jelly (Firmness): how fast it springs back, a wobble
  // of about 24 frames at 0 to about 4 at 1.
  double spring = 0.4;

  bool operator==(const DragNode&) const = default;
};

// How a warp point moves by itself, on top of its keys and drag nodes:
// hair that waves, a heart that beats.
enum class WarpMotionKind {
  kNone,
  kWave,     // A swing that reaches its neighbours late, a ripple.
  kPulse,    // A lub-dub out along its direction, for a heart.
  kBreathe,  // A slow smooth swell out and back, for a chest.
  kSway,     // A swing to and fro, neighbours in step, for cloth.
  kShiver,   // Small fast shakes every way, for fear or cold.
};
constexpr int kWarpMotionKindCount = 6;

// Frames one cycle of a point motion may take, 24 being a second.
constexpr int kMinWarpCycle = 2;
constexpr int kMaxWarpCycle = 240;
// The farthest a point motion pushes its point, in drawing pixels.
constexpr double kMaxWarpMotionSize = 500.0;

// A warp grid point that moves by itself. Its neighbours follow by the
// point's rubber reach, as they follow a drag node.
struct PointMotion final {
  int point = 0;
  WarpMotionKind kind = WarpMotionKind::kSway;
  // How far the point moves at most, in drawing pixels.
  double size = 8.0;
  // Frames one cycle takes.
  int cycle = 24;
  // The way it moves, in degrees: 0 is right, 90 is down.
  double angle = 0.0;
  // How late in its cycle it starts, 0 to 1 of a cycle.
  double delay = 0.0;

  bool operator==(const PointMotion&) const = default;
};

// The most dots a collider outline has.
constexpr int kMaxColliderDots = 96;

// One dot of a collider's outline, in the piece's drawing pixels.
// slot is its place on the ring the outline was made with.
struct ColliderDot final {
  QPointF at;
  int slot = 0;

  bool operator==(const ColliderDot&) const = default;
};

// A piece's outline that pushes the warp grid points of pieces drawn in
// front of it, so a leg never passes through a skirt: a point (or the
// drawn edge between two) that the outline comes into, against where it
// was as drawn, is pushed back out. What overlaps as drawn stays. The
// ring was traced from the drawing's visible pixels, ring_size slots in
// all; dots the user deleted leave gaps, and an edge joins two dots
// only when they are next to each other on the ring. Empty dots mean
// no collider.
struct Collider final {
  int ring_size = 0;
  std::vector<ColliderDot> dots;
  // Pieces it does not push.
  std::vector<QString> spared;

  bool IsOn() const { return !dots.empty(); }
  bool operator==(const Collider&) const = default;
};

// The settings of a piece's warp dots that belong to one place the doll
// is used (one layer of a shot) instead of to the doll: rubber reach,
// drag nodes and point motions. Each is what RigPiece holds by the same
// name.
struct PieceDots final {
  std::vector<double> warp_reach{};
  std::vector<DragNode> drag_nodes{};
  std::vector<PointMotion> point_motions{};

  bool operator==(const PieceDots&) const = default;
};

// A drawing made in Snapper's draw mode. It is kept in the rig, because
// art.json is Krita's and a re-export rewrites it; loading a doll lists
// it after the piece's Krita drawings (AddOwnDrawings), so keys, the
// Drawing picker and dial stops use it like any other drawing.
struct OwnDrawing final {
  // The PNG inside the doll folder ("snapper/mouth 2.png").
  QString file;
  QString name;

  bool operator==(const OwnDrawing&) const = default;
};

// A disc drawn at a piece's joint, just under the piece, as wide as the
// drawn limb is there (measured, never stored), so a bent joint shows
// a round elbow instead of a gap.
struct JointCircle final {
  bool is_on = false;
  // Invalid until set; turning the circle on sets it to the drawing's
  // most common color at the joint.
  QColor color{};

  bool operator==(const JointCircle&) const = default;
};

// How Snapper moves a piece: rig.json.
struct RigPiece final {
  QString name;
  // Empty for a root piece.
  QString parent;
  // The joint, in the piece's own drawing pixels. The piece turns and
  // scales around it, and hangs from its parent there.
  QPointF pivot;
  // Higher draws in front.
  int order = 0;
  // -1 uses the art's default drawing.
  int default_drawing = -1;
  WarpGrid warp;
  // Degrees the piece is turned at rest, on top of how it was drawn;
  // poses turn from there.
  double rest_rotation = 0.0;
  // Leaning or swivelling the doll moves and sizes the piece but never
  // squashes its drawing: for heads and the like.
  bool keeps_shape = false;
  // How far, in grid cells, pulling each warp grid point drags its
  // neighbours along, like rubber; 0 moves the point alone. Row by row,
  // one per point, or empty when every point is 0.
  std::vector<double> warp_reach{};
  // Grid points that drag behind, each at most once.
  std::vector<DragNode> drag_nodes{};
  // Grid points that move by themselves, each at most once.
  std::vector<PointMotion> point_motions{};
  // The outline that pushes the other pieces of the doll, if any.
  Collider collider{};
  // The warp dot settings of the layers that changed them for
  // themselves, by layer: editing a shot's doll never touches another
  // shot's. The fields above are what every other layer shows, and what
  // a new one starts with (the doll's default).
  std::map<LayerId, PieceDots> layer_dots{};
  // Drawings made in Snapper, oldest first.
  std::vector<OwnDrawing> own_drawings{};
  JointCircle joint_circle{};

  bool operator==(const RigPiece&) const = default;
};

// Two pieces bent by dragging a point at the end of the lower one,
// such as upper arm, forearm and the hand's spot.
struct IkChain final {
  QString name;
  QString upper;
  QString lower;
  // The dragged point, in the lower piece's drawing pixels.
  QPointF tip;
  // Which way the middle joint bends.
  bool bends_clockwise = true;

  bool operator==(const IkChain&) const = default;
};

struct Rig final {
  std::vector<RigPiece> pieces;
  std::vector<IkChain> chains;
  // Dot groups every shot shows for this doll (their dots have no layer).
  std::vector<DotGroup> dot_groups;
  // Where Doll to Krita last wrote this doll's Krita file; empty when
  // it never did.
  QString krita_file;
  // Colors added to the palette by hand for this doll, oldest first.
  std::vector<QColor> added_colors;
  // Smart dials, in the order they are listed; for the drawing, a dial
  // later in the list wins.
  std::vector<SmartDial> dials;

  bool operator==(const Rig&) const = default;
};

// A doll as the project holds it: Krita's art and Snapper's rig, with
// one rig piece for every art piece of the same name.
struct Doll final {
  QString name;
  // Where the drawings live.
  QString folder;
  DollArt art;
  Rig rig;

  bool operator==(const Doll&) const = default;
};

const ArtPiece* FindArt(const Doll& doll, const QString& name);
// The size of the piece's drawings, or an empty size when it has none.
QSize ArtSizeOf(const Doll& doll, const QString& name);
const RigPiece* FindRig(const Rig& rig, const QString& name);
RigPiece* FindRig(Rig* rig, const QString& name);
// The motion of rig's warp point, or nullptr when it has none.
const PointMotion* FindMotion(const RigPiece& rig, int point);
// The rubber reach of rig's warp point, 0 when it has none.
double ReachOf(const RigPiece& rig, int point);
const IkChain* FindChain(const Rig& rig, const QString& name);

// The warp dot settings piece shows on layer: the layer's own when it
// has changed them, else the piece's (the doll's default).
PieceDots DotsOf(const RigPiece& piece, LayerId layer);
// The drag nodes piece has on layer (no copy).
const std::vector<DragNode>& DragNodesOf(const RigPiece& piece, LayerId layer);
// Makes piece's fields hold the dots layer shows, so code that reads
// the fields sees that layer's settings.
void ShowDots(const PieceDots& dots, RigPiece* piece);
// piece with the dots layer shows put in its fields.
RigPiece PieceFor(const RigPiece& piece, LayerId layer);
// doll with every piece as PieceFor gives it. Copies nothing when no
// piece has layer's own settings.
class DollFor final {
 public:
  DollFor(const Doll& doll, LayerId layer);
  const Doll& get() const { return own_ ? *own_ : *shared_; }

 private:
  const Doll* shared_;
  std::unique_ptr<Doll> own_;
};

// True when parent may become child's parent: it exists (or is empty,
// meaning root) and the rig stays a tree.
bool CanParent(const Rig& rig, const QString& child, const QString& parent);

// The drawing a piece shows for a pose's drawing choice, as a full
// path; empty if the piece has no such drawing.
QString DrawingPath(const Doll& doll, const QString& piece, int drawing);

// The own drawing (made in Snapper) that is drawing index of piece, or
// nullptr for a Krita drawing.
const OwnDrawing* OwnDrawingAt(const Doll& doll, const QString& piece,
                               int index);

}  // namespace snapper

#endif  // SNAPPER_MODEL_DOLL_H_
