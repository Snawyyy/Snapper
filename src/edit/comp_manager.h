#ifndef SNAPPER_EDIT_COMP_MANAGER_H_
#define SNAPPER_EDIT_COMP_MANAGER_H_

#include <QColor>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QTransform>

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "base/error.h"
#include "edit/selection_manager.h"
#include "base/frame.h"
#include "model/comp.h"
#include "model/shot.h"
#include "render/comp.h"

namespace snapper {

class HistoryManager;

// How the panel shows one control of an effect.
enum class CompRowKind {
  kFader,   // A keyed number on a fader.
  kDial,    // A keyed angle on a dial.
  kToggle,  // A plugin's checkbox: 0 or 1, not keyed.
  kChoice,  // A plugin's popup: the choice's number from 1, not keyed.
  kColor,   // A plugin's color: 0xRRGGBB, not keyed.
};

// One entry of the Comp tab's effect library: a kind, a plugin by id
// (kFsp) or the Scene gradient.
struct CompEntry final {
  CompKind kind = CompKind::kRimLight;
  QString plugin{};
  bool is_scene_gradient = false;

  bool operator==(const CompEntry&) const = default;
};

// How many recently used library entries are kept.
inline constexpr int kMaxRecentEntries = 6;

// One effect clip on the Comp timeline: an effect of a shot.
struct CompClipRef final {
  ShotId shot{};
  CompEffectId id{};

  auto operator<=>(const CompClipRef&) const = default;
};

// An index that means the top of a stack.
inline constexpr int kOnTop = -1;

// One control of an effect's panel, in the order shown.
struct CompRow final {
  CompKnob knob = CompKnob::kAmount;
  CompRowKind kind = CompRowKind::kFader;
  QString label{};
  // kChoice: the entries, the first one is value 1.
  QStringList choices{};
  // Number fields: digits after the point and the step of an arrow.
  int decimals = 0;
  double step = 1.0;
  // Has a Key button (numbers only).
  bool is_keyable = true;
};

// The Comp tab's finishing effects (model/comp.h): every edit to a
// shot's comp stack and its Light, each one named undo step through the
// history ("Rim light width"). Keyed knobs follow one rule: with no key
// or one, a change sets the knob for the whole shot; once it has two
// keys or more, a change keys it at the frame (ToggleKey adds or drops
// one). Also holds the tab's view state, never saved: the picked effect
// per shot, V (all effects off), K (show the picked effect's mask) and
// what each effect cost on the stage's last frame.
class CompManager final : public QObject {
  Q_OBJECT

 public:
  explicit CompManager(HistoryManager* history);

  // Words for the panels.
  static QString KindName(CompKind kind);
  static QString KindHelp(CompKind kind);
  // The effect's own name: its kind's, or its plugin's ("RimFill"), or
  // the missing plugin's id marked as missing.
  static QString EffectName(const CompEffect& effect);
  static QString EffectHelp(const CompEffect& effect);
  // What the plugin with id does, in a line.
  static QString PluginHelp(const QString& id);
  static QString BlendName(BlendMode mode);
  static QString ShapeName(GradientShape shape);
  // The effect's label for knob ("Width", a plugin's own word); empty
  // when it does not use it.
  static QString KnobLabel(const CompEffect& effect, CompKnob knob);
  // The controls the panel shows for effect as it is set now, in order:
  // the shown knobs (IsKnobShown), or a plugin's (or the Line
  // smoother's) Strength and then its own parameters, hidden ones and
  // ones its switches make unused left out. None for a missing plugin.
  static std::vector<CompRow> RowsOf(const CompEffect& effect);
  // True when knob may have keys (a plugin's toggles, choices and
  // colors hold one value for the shot).
  static bool IsKeyable(const CompEffect& effect, CompKnob knob);
  // The kind's variants ("Outside", "Inside"); empty for none.
  static std::vector<QString> StyleNames(CompKind kind);
  // True when the panel shows knob as a fader or dial for effect as it
  // is set now (points are stage handles; a knob its look ignores, or
  // the angle while the shot's Light aims it, is left out).
  static bool IsKnobShown(const CompEffect& effect, CompKnob knob);
  // True when the effect uses its color (lights, sparkles, lines).
  static bool UsesColor(const CompEffect& effect);
  // True when it can follow the shot's Light.
  static bool CanUseShotLight(const CompEffect& effect);

  // The stack, bottom first; empty when the shot is gone.
  std::vector<CompEffect> Of(ShotId shot) const;
  const CompEffect* Find(ShotId shot, CompEffectId id) const;

  // Why Add can't act; empty when it can.
  QString WhyNoAdd(ShotId shot) const;
  // Adds a kind's effect at index in the stack (0 bottom, kOnTop on
  // top) with its own defaults, working on target (the screen when
  // empty); it is picked.
  Result<CompEffectId> Add(ShotId shot, CompKind kind,
                           const CompTarget& target = {},
                           int index = kOnTop);
  // Why the plugin with id can't be an effect (not in this Snapper, too
  // many values); empty when it can. Shot fullness is WhyNoAdd's.
  static QString WhyNoPlugin(const QString& id);
  // Adds an F's Plugins effect (kFsp) with the plugin's own defaults,
  // like Add.
  Result<CompEffectId> AddPlugin(ShotId shot, const QString& id,
                                 const CompTarget& target = {},
                                 int index = kOnTop);
  // A gradient preset in one click: Overlay, warm at the top to cool at
  // the bottom, on target like Add.
  Result<CompEffectId> AddSceneGradient(ShotId shot,
                                        const CompTarget& target = {},
                                        int index = kOnTop);
  // Adds a library entry the way its own Add does (edit/comp_clip.cpp)
  // and puts it first in the recently used.
  Result<CompEffectId> AddEntry(ShotId shot, const CompEntry& entry,
                                const CompTarget& target, int index);
  // The library entries added last, newest first; the library keeps
  // them in the settings file between runs.
  const std::vector<CompEntry>& recent_entries() const {
    return recent_entries_;
  }
  void SetRecentEntries(std::vector<CompEntry> entries);
  // Ctrl+C: keeps a copy of the effect, for any shot.
  Result<void> Copy(ShotId shot, CompEffectId id);
  // Ctrl+C on the timeline: keeps a copy of every clip, with where each
  // sits on the reel (edit/comp_cut.cpp).
  Result<void> CopyClips(const std::set<CompClipRef>& clips);
  // The copied effect's name ("3 effects" for many); empty with nothing
  // copied.
  QString CopiedName() const;
  QString WhyNoPaste(ShotId shot) const;
  // The copies at index (kOnTop on top), working on target with their
  // own picked colors and timing; the last one is picked.
  Result<CompEffectId> Paste(ShotId shot, const CompTarget& target,
                             int index = kOnTop);
  // Ctrl+V on the Comp tab: the copies as clips of the shot at the reel
  // frame, the first one starting there and the rest as far after it as
  // they were. A copy keeps its target in its own shot and takes
  // elsewhere in another one. The pasted clips are picked.
  QString WhyNoPasteAt(Frame reel) const;
  Result<void> PasteAt(Frame reel, const CompTarget& elsewhere);
  // S: cuts every clip the reel frame is inside in two there, as one
  // undo step; the right halves are picked.
  QString WhyNoSplit(const std::set<CompClipRef>& clips, Frame reel) const;
  Result<void> SplitAll(const std::set<CompClipRef>& clips, Frame reel);
  // Ctrl+X: copies the clips, then removes them, as one undo step.
  Result<void> CutAll(const std::set<CompClipRef>& clips);
  // Ctrl+D: a copy of each just above it, as one undo step; the copies
  // are picked.
  Result<void> DuplicateAll(const std::set<CompClipRef>& clips);
  Result<void> Remove(ShotId shot, CompEffectId id);
  // A copy just above it, picked.
  Result<CompEffectId> Duplicate(ShotId shot, CompEffectId id);
  // Moves it to index in the stack (0 bottom).
  Result<void> Move(ShotId shot, CompEffectId id, int index);
  // The eye on its row.
  Result<void> SetOn(ShotId shot, CompEffectId id, bool is_on);
  Result<void> SetBlend(ShotId shot, CompEffectId id, BlendMode mode);
  // Shift+= and Shift+-: the next or previous mode, wrapping.
  Result<void> StepBlend(ShotId shot, CompEffectId id, int delta);

  // Times the effect from start for length frames of its shot
  // (edit/comp_timing.cpp); kWholeShot follows the shot again. With
  // carries_keys its keys move by as much as the start did, as when its
  // clip is dragged whole.
  Result<void> SetTiming(ShotId shot, CompEffectId id, int start, int length,
                         bool carries_keys);
  // Why the effect's timing can't be changed; empty when it can.
  QString WhyNoTiming(ShotId shot, CompEffectId id) const;

  // Sets knob at frame (see the class comment), clamped to its range.
  Result<void> SetKnob(ShotId shot, CompEffectId id, CompKnob knob,
                       Frame frame, double value);
  // Point A or B (canvas pixels from the middle, or the target layer's
  // own space), keyed the same way ("Move Gradient point").
  Result<void> SetPoint(ShotId shot, CompEffectId id, bool is_b, Frame frame,
                        QPointF point);
  // Adds a key at frame holding the value there, or drops the key there.
  Result<void> ToggleKey(ShotId shot, CompEffectId id, CompKnob knob,
                         Frame frame);
  bool HasKeyAt(ShotId shot, CompEffectId id, CompKnob knob,
                Frame frame) const;
  // Keys hold unless eased: this eases (or holds) every key of it.
  Result<void> SetEased(ShotId shot, CompEffectId id, bool is_eased);
  static bool IsEased(const CompEffect& effect);
  // Every frame any knob of it has a key on.
  static std::set<int> KeyFrames(const CompEffect& effect);

  Result<void> SetColor(ShotId shot, CompEffectId id, QColor color);
  // A plugin's color knob (kColor row): the whole shot.
  Result<void> SetKnobColor(ShotId shot, CompEffectId id, CompKnob knob,
                            QColor color);
  // The color a kColor knob holds at frame.
  static QColor KnobColor(const CompEffect& effect, CompKnob knob,
                          Frame frame);
  Result<void> SetStyle(ShotId shot, CompEffectId id, int style);
  Result<void> SetShape(ShotId shot, CompEffectId id, GradientShape shape);
  Result<void> SetUsesShotLight(ShotId shot, CompEffectId id, bool is_used);
  Result<void> SetStopColor(ShotId shot, CompEffectId id, int stop,
                            QColor color);
  QString WhyNoAddStop(const CompEffect& effect) const;
  QString WhyNoRemoveStop(const CompEffect& effect) const;
  // A stop in the middle of the widest gap; stops spread evenly.
  Result<void> AddStop(ShotId shot, CompEffectId id);
  Result<void> RemoveStop(ShotId shot, CompEffectId id, int stop);
  QString WhyNoAddSwap(const CompEffect& effect) const;
  // A pair from color to white.
  Result<void> AddSwap(ShotId shot, CompEffectId id, QColor from);
  Result<void> SetSwapColor(ShotId shot, CompEffectId id, int swap, bool is_to,
                            QColor color);
  Result<void> RemoveSwap(ShotId shot, CompEffectId id, int swap);

  // A target's words ("Screen", "Mika: arm_l, 2 colors").
  QString TargetText(ShotId shot, const CompTarget& target) const;
  // A target in short, to put before an effect's name: "(arm_l)",
  // "(Mika)", "(screen)".
  QString TargetTag(ShotId shot, const CompTarget& target) const;
  // The target a new effect gets from what is picked on the Comp stage
  // (edit/comp_pick.cpp): no layer is the screen, a layer the whole
  // layer. Fails on a layer that draws nothing.
  Result<CompTarget> TargetFor(ShotId shot, LayerId layer) const;
  // The effects the Comp tab lists for that pick of shot, at the reel
  // frame: with no layer the screen effects running there, of any shot,
  // and every screen effect of shot; with a layer every effect on it.
  // Shots by where their clip starts, each in the order they run.
  std::vector<CompClipRef> ShownFor(ShotId shot, LayerId layer,
                                    Frame reel) const;
  // A palette or stage pick: alone, Shift adds it, Ctrl flips it.
  Result<void> PickTargetColor(ShotId shot, CompEffectId id, QColor color,
                               Qt::KeyboardModifiers modifiers);
  Result<void> ClearTargetColors(ShotId shot, CompEffectId id);

  // The shot's Light at frame, and setting it, keyed like a knob.
  double ShotLight(ShotId shot, Frame frame) const;
  Result<void> SetShotLight(ShotId shot, Frame frame, double angle);
  Result<void> ToggleShotLightKey(ShotId shot, Frame frame);
  bool HasShotLightKeyAt(ShotId shot, Frame frame) const;

  // Where an effect's points land on a frame drawn at scale (output
  // pixels), the same space the renderer uses.
  QTransform PointSpace(ShotId shot, CompEffectId id, Frame frame,
                        double scale) const;

  // The picked effect of shot; invalid for none.
  CompEffectId PickedIn(ShotId shot) const;
  // Picks one effect, alone among the timeline's picked clips.
  void Pick(ShotId shot, CompEffectId id);
  // The effect keys and handles act on: the one last picked, of any
  // shot; invalid for none or when it is gone.
  CompClipRef Focus() const;
  // The effect clips picked on the Comp timeline, of any shots; those
  // gone from the project are left out (edit/comp_clip_picks.cpp).
  std::set<CompClipRef> ClipPicks() const;
  // Combines clips into the pick as mode says (the shared pick rule);
  // focus, when picked after, becomes its shot's picked effect.
  void PickClips(const std::set<CompClipRef>& clips, PickMode mode,
                 CompClipRef focus);
  // Every effect clip of every shot.
  std::set<CompClipRef> AllClips() const;
  // V held: every effect off on the Comp stage.
  bool are_effects_off() const { return are_effects_off_; }
  void SetEffectsOff(bool is_off);
  // K held: the picked effect's mask on the Comp stage.
  bool is_mask_shown() const { return is_mask_shown_; }
  void SetMaskShown(bool is_shown);
  // What the Comp stage should draw for shot.
  CompShow ShowFor(ShotId shot) const;
  // The stage's last frame cost per effect, for the slow clock, and the
  // plugin effects that refused to run.
  void NoteCosts(const CompReport& report);
  // True when it took longer than a frame at 24 fps last time.
  bool IsSlow(CompEffectId id) const;
  // Why effect draws nothing on the stage (its plugin is missing, or it
  // refused to run on the last frame); empty when it runs.
  QString WhyNotRun(const CompEffect& effect) const;

 signals:
  // The pick, V, K or the costs changed; edits come through the
  // history's Changed.
  void Changed();

 private:
  // IsKnobShown for a plugin effect or the Line smoother: Strength, and
  // the slots of its shown number parameters.
  static bool IsPluginKnobShown(const CompEffect& effect, CompKnob knob);
  // Applies change(CompEffect*) to one effect as an undo step.
  Result<void> Edit(ShotId shot, CompEffectId id, const QString& label,
                    const std::function<Result<void>(CompEffect*)>& change);
  Result<CompEffectId> Insert(ShotId shot, CompEffect effect, int index,
                              const QString& label);
  // A clip's frames: of its shot, and the reel frame the first one
  // shows on.
  struct ClipFrames final {
    CompSpan local{};
    int reel_first = 0;
  };
  // nullopt when the effect or its shot's clip on the reel is gone.
  std::optional<ClipFrames> FramesOf(const CompClipRef& clip) const;

  // A copied effect as it was, and its clip's frames then (none when
  // its shot was off the reel).
  struct Copied final {
    CompEffect effect{};
    ShotId shot{};
    std::optional<ClipFrames> frames{};
  };

  HistoryManager* history_;
  std::map<ShotId, CompEffectId> picked_;
  std::set<CompClipRef> clip_picks_;
  CompClipRef focus_{};
  bool are_effects_off_ = false;
  bool is_mask_shown_ = false;
  std::map<CompEffectId, double> costs_;
  std::set<CompEffectId> failed_;
  std::vector<Copied> copied_;
  std::vector<CompEntry> recent_entries_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_COMP_MANAGER_H_
