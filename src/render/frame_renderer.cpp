#include "render/frame_renderer.h"

#include <QPainter>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include <set>
#include <utility>
#include <variant>
#include <vector>

#include "anim/doll_pose.h"
#include "anim/jitter.h"
#include "anim/layer_order.h"
#include "anim/links.h"
#include "anim/master_timeline.h"
#include "anim/reel_timeline.h"
#include "anim/sampler.h"
#include "render/comp.h"
#include "render/storyboard_paint.h"
#include "render/effects.h"
#include "render/layer_painter.h"

namespace snapper {

QImage FrameRenderer::Blank(const Project& project, double scale,
                            QColor color) {
  assert(scale > 0.0);
  assert(IsValidCanvas(project.canvas));
  const QSize size(
      std::max(1, static_cast<int>(std::lround(project.canvas.width * scale))),
      std::max(1,
               static_cast<int>(std::lround(project.canvas.height * scale))));
  QImage image(size, QImage::Format_ARGB32_Premultiplied);
  image.fill(color);
  return image;
}

namespace {

// Cuts frame back to where before had something drawn, keeping soft
// edges soft (both premultiplied).
void KeepDrawnOnly(const QImage& before, QImage* frame) {
  assert(frame != nullptr);
  assert(before.size() == frame->size());
  QPainter painter(frame);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(0, 0, before);
}

// ViewTransform, with or without the camera's shake.
QTransform ViewOf(const Project& project, const Shot& shot, Frame local,
                  double scale, bool is_shaken) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  const CameraPose camera = Sample(shot.camera, local, CameraPose());
  const double shaking = is_shaken ? camera.shake : 0.0;
  const QPointF shake = QPointF(Jitter(local.index(), 0),
                                Jitter(local.index(), 1)) * shaking;
  const QPointF look = camera.center + shake;
  QTransform view = QTransform::fromTranslate(-look.x(), -look.y());
  view *= QTransform().rotate(-camera.rotation);
  view *= QTransform::fromScale(camera.zoom, camera.zoom);
  view *= QTransform::fromTranslate(project.canvas.width / 2.0,
                                    project.canvas.height / 2.0);
  return view * QTransform::fromScale(scale, scale);
}

// An effect layer over what is drawn so far.
void RunEffectLayer(const Shot& shot, const Layer& layer,
                    const EffectLayer& effect, Frame local, double scale,
                    QImage* frame) {
  assert(frame != nullptr);
  assert(scale > 0.0);
  const double opacity = Sample(layer.transform, local, PiecePose()).opacity;
  // On a see-through shot an effect acts only where something is
  // drawn; a zoom punch moves what is drawn, so it is left alone.
  const bool is_masked =
      shot.is_transparent && effect.kind != EffectKind::kZoomPunch;
  const QImage before = is_masked ? frame->copy() : QImage();
  ApplyEffect(effect.kind, Sample(effect.amount, local, 1.0) * opacity,
              effect.color, scale, local.index(), frame);
  if (is_masked) {
    KeepDrawnOnly(before, frame);
  }
}

// One thing RenderShot draws, back to front: a layer, or one part of a
// doll layer that a storyboard sinks into (only those pieces).
struct DrawStep final {
  const Layer* layer = nullptr;
  std::optional<std::set<QString>> part;
  bool is_first_part = true;
};

// How many limbs layer sinks behind: its sink when it is a storyboard
// this renderer draws, else 0.
int SinkOf(const Layer& layer, bool shows_boards) {
  const auto* board = std::get_if<StoryboardLayer>(&layer.content);
  const bool is_drawn =
      board != nullptr && (shows_boards || board->is_exported);
  return is_drawn ? std::clamp(board->sink, 0, kMaxStoryboardSink) : 0;
}

// drawn (layers back to front) as draw steps. A doll with storyboards
// right over it, one of which sinks, is cut into parts at each
// storyboard's sink, counted in drawn limbs from the front at local,
// and each storyboard is drawn between its parts. Storyboards sinking
// equally deep keep their stack order.
std::vector<DrawStep> DrawSteps(const Project& project,
                                const std::vector<const Layer*>& drawn,
                                Frame local, LinkSolver* links,
                                ImageCache* cache, bool shows_boards) {
  assert(links != nullptr && cache != nullptr);
  assert(drawn.size() <= static_cast<size_t>(kMaxLayersPerShot));
  std::vector<DrawStep> steps;
  const size_t count = drawn.size();
  size_t at = 0;
  while (at < count) {
    const Layer& layer = *drawn[at];
    ++at;
    const auto* posed = std::get_if<DollLayer>(&layer.content);
    const Doll* doll =
        posed != nullptr ? FindDoll(project, posed->doll) : nullptr;
    size_t end = at;
    bool has_sink = false;
    while (end < count &&
           std::holds_alternative<StoryboardLayer>(drawn[end]->content)) {
      has_sink = has_sink || SinkOf(*drawn[end], shows_boards) > 0;
      ++end;
    }
    const std::vector<PlacedPiece> placed =
        doll != nullptr && has_sink
            ? PlaceDoll(*doll, links->Poses(*doll, layer, local))
            : std::vector<PlacedPiece>();
    const bool is_sunk_into = !placed.empty();
    if (!is_sunk_into) {
      steps.push_back({&layer, std::nullopt, true});
      continue;
    }
    // The drawn limbs, back to front, as places in placed: a sink
    // counts only limbs that show.
    std::vector<size_t> shown;
    for (size_t i = 0; i < placed.size(); ++i) {
      const bool is_shown = placed[i].opacity > 0.0 &&
                            !cache->Get(placed[i].drawing).isNull();
      if (is_shown) {
        shown.push_back(i);
      }
    }
    // Each storyboard with where the part in front of it starts.
    std::vector<std::pair<size_t, const Layer*>> cuts;
    for (size_t i = at; i < end; ++i) {
      const size_t sink = std::min(
          static_cast<size_t>(SinkOf(*drawn[i], shows_boards)), shown.size());
      const size_t cut =
          sink == 0 ? placed.size() : shown[shown.size() - sink];
      cuts.emplace_back(cut, drawn[i]);
    }
    std::stable_sort(cuts.begin(), cuts.end(),
                     [](const auto& a, const auto& b) {
                       return a.first < b.first;
                     });
    size_t from = 0;
    bool is_first = true;
    const auto add_part = [&](size_t to) {
      const bool is_empty = to <= from;
      if (is_empty) {
        return;
      }
      std::set<QString> part;
      for (size_t i = from; i < to; ++i) {
        part.insert(placed[i].name);
      }
      steps.push_back({&layer, std::move(part), is_first});
      is_first = false;
      from = to;
    };
    for (const auto& [cut, board] : cuts) {
      add_part(cut);
      steps.push_back({board, std::nullopt, true});
    }
    add_part(placed.size());
    at = end;
  }
  assert(steps.size() >= count);
  return steps;
}

}  // namespace

QTransform FrameRenderer::ViewTransform(const Project& project,
                                        const Shot& shot, Frame local,
                                        double scale) {
  assert(scale > 0.0);
  assert(local.index() >= 0);
  return ViewOf(project, shot, local, scale, true);
}

QImage FrameRenderer::RenderShot(const Project& project, const Shot& shot,
                                 Frame local, double scale,
                                 const QImage* under, bool runs_screen,
                                 QImage* drawn) {
  assert(scale > 0.0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  QImage frame = Blank(project, scale,
                       shot.is_transparent ? QColor(Qt::transparent)
                                           : shot.background);
  const bool has_under = under != nullptr && !under->isNull();
  if (has_under) {
    QPainter painter(&frame);
    painter.drawImage(0, 0, *under);
  }
  const QTransform view = ViewTransform(project, shot, local, scale);
  LinkSolver links(project, shot, cache_.poses());
  CompNeeds needs = NeedsOf(shot, local, comp_show_, runs_screen);
  const bool wants_drawn = drawn != nullptr;
  needs.needs_drawn = needs.needs_drawn || wants_drawn;
  CompPictures pictures;
  const bool needs_drawn = needs.needs_drawn;
  if (needs_drawn) {
    pictures.drawn = Blank(project, scale, Qt::transparent);
  }
  for (const DrawStep& step :
       DrawSteps(project, LayersInDrawOrder(shot, local), local, &links,
                 &cache_, is_storyboard_shown_)) {
    const Layer& layer = *step.layer;
    const std::set<QString>* part =
        step.part.has_value() ? &*step.part : nullptr;
    const auto* effect = std::get_if<EffectLayer>(&layer.content);
    const bool is_effect = effect != nullptr;
    if (is_effect) {
      RunEffectLayer(shot, layer, *effect, local, scale, &frame);
      continue;
    }
    const bool is_board = PaintBoardLayer(layer, local, &frame);
    if (is_board) {
      continue;
    }
    const bool is_split = !needs.IsEmpty();
    if (is_split) {
      PaintForComp(project, layer, local, scale, view, &links, needs,
                   &pictures, &frame, part, step.is_first_part);
      continue;
    }
    QPainter painter(&frame);
    PaintLayer(project, layer, local, view, &links, &cache_, &painter, part,
               PieceSplit::kPart);
  }
  if (wants_drawn) {
    *drawn = pictures.drawn;
  }
  const bool has_comp = !shot.comp.empty();
  if (has_comp) {
    CompContext context{&project, &shot,     local,       scale,
                        view,     &links,    &pictures,   comp_show_};
    context.skips_screen = !runs_screen;
    ApplyComp(context, &frame, &comp_report_);
  }
  return frame;
}

bool FrameRenderer::PaintBoardLayer(const Layer& layer, Frame local,
                                    QImage* frame) {
  assert(frame != nullptr);
  assert(local.index() >= 0);
  const auto* board = std::get_if<StoryboardLayer>(&layer.content);
  const bool is_board = board != nullptr;
  if (!is_board) {
    return false;
  }
  const bool is_drawn = is_storyboard_shown_ || board->is_exported;
  if (is_drawn) {
    PaintStoryboard(*board, local, &cache_, frame);
  }
  return true;
}

void FrameRenderer::PaintForComp(const Project& project, const Layer& layer,
                                 Frame local,
                                 double scale, const QTransform& view,
                                 LinkSolver* links, const CompNeeds& needs,
                                 CompPictures* pictures, QImage* frame,
                                 const std::set<QString>* part,
                                 bool is_first_part) {
  assert(pictures != nullptr && frame != nullptr);
  assert(!needs.IsEmpty());
  assert(part != nullptr || is_first_part);
  QImage own = Blank(project, scale, Qt::transparent);
  {
    QPainter painter(&own);
    PaintLayer(project, layer, local, view, links, &cache_, &painter, part,
               PieceSplit::kPart);
  }
  // What this layer covers is cut out of the targets below it and joins
  // their covers, so an effect on a doll stays behind what stands in
  // front of it. A later part of a doll leaves the doll's own targets
  // alone: they were made whole with its first part.
  for (auto& [key, picture] : pictures->targets) {
    const bool is_own_later = !is_first_part && key.layer == layer.id;
    if (is_own_later) {
      continue;
    }
    QPainter painter(&picture);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.drawImage(0, 0, own);
    QImage& cover = pictures->covers[key];
    const bool is_first_cover = cover.isNull();
    if (is_first_cover) {
      cover = Blank(project, scale, Qt::transparent);
    }
    QPainter covering(&cover);
    covering.drawImage(0, 0, own);
  }
  for (const CompTargetKey& key : needs.targets) {
    const bool is_this = key.layer == layer.id;
    if (!is_this) {
      continue;
    }
    const bool is_whole = key.pieces.empty();
    if (is_whole) {
      const auto made = pictures->targets.find(key);
      const bool is_added = !is_first_part && made != pictures->targets.end();
      if (is_added) {
        QPainter adding(&made->second);
        adding.drawImage(0, 0, own);
      } else {
        pictures->targets[key] = own;
      }
      continue;
    }
    if (!is_first_part) {
      continue;
    }
    // The doll's own pieces above the picked ones cover them too.
    const std::set<QString> only(key.pieces.begin(), key.pieces.end());
    for (const PieceSplit split : {PieceSplit::kShown, PieceSplit::kCover}) {
      QImage picture = Blank(project, scale, Qt::transparent);
      QPainter painter(&picture);
      PaintLayer(project, layer, local, view, links, &cache_, &painter, &only,
                 split);
      painter.end();
      const bool is_shown = split == PieceSplit::kShown;
      (is_shown ? pictures->targets : pictures->covers)[key] = picture;
    }
  }
  QPainter painter(frame);
  painter.drawImage(0, 0, own);
  const bool has_drawn = !pictures->drawn.isNull();
  if (has_drawn) {
    QPainter drawn(&pictures->drawn);
    drawn.drawImage(0, 0, own);
  }
}

QImage FrameRenderer::RenderLayers(
    const Project& project, const Shot& shot, Frame local, double scale,
    const std::map<LayerId, std::set<QString>>& only) {
  assert(scale > 0.0);
  assert(shot.layers.size() <= static_cast<size_t>(kMaxLayersPerShot));
  QImage frame = Blank(project, scale, Qt::transparent);
  const QTransform view = ViewTransform(project, shot, local, scale);
  LinkSolver links(project, shot, cache_.poses());
  QPainter painter(&frame);
  for (const Layer* layer : LayersInDrawOrder(shot, local)) {
    const auto chosen = only.find(layer->id);
    const bool is_chosen = chosen != only.end() && IsStagePicture(*layer);
    if (is_chosen) {
      const bool is_whole = chosen->second.empty();
      PaintLayer(project, *layer, local, view, &links, &cache_, &painter,
                 is_whole ? nullptr : &chosen->second);
    }
  }
  return frame;
}

QImage FrameRenderer::RenderArea(const Project& project, const Shot& shot,
                                 Frame local, double scale,
                                 const QRect& area) {
  assert(scale > 0.0);
  assert(area.width() > 0 && area.height() > 0);
  QImage out(area.size(), QImage::Format_ARGB32_Premultiplied);
  out.fill(Qt::transparent);
  const QTransform view = ViewOf(project, shot, local, scale, false) *
                          QTransform::fromTranslate(-area.left(),
                                                    -area.top());
  LinkSolver links(project, shot, cache_.poses());
  QPainter painter(&out);
  for (const Layer* layer : LayersInDrawOrder(shot, local)) {
    const bool is_drawn = IsStagePicture(*layer);
    if (is_drawn) {
      PaintLayer(project, *layer, local, view, &links, &cache_, &painter);
    }
  }
  return out;
}

QImage FrameRenderer::RenderFrame(const Project& project, Frame master,
                                  double scale, const QImage* under) {
  assert(scale > 0.0);
  assert(master.index() >= 0);
  const ShotMoment moment = Locate(project, master);
  const bool is_past_end = moment.shot < 0;
  if (is_past_end) {
    return Blank(project, scale, Qt::black);
  }
  const Shot& shot = *project.shots[static_cast<size_t>(moment.shot)];
  return RenderShot(project, shot, moment.local, scale, under);
}

}  // namespace snapper
