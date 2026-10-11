# Feature map

Find the row for your change, read only the named `##` section of
`ARCHITECTURE.md` (Grep the heading for its line, then Read from
there), edit in the named manager, add the test in the named file.
Paths are under `src/` and `tests/`. The Dolls section is long and has
no sub-headings: search it for the feature word.

Keep this file true: a new feature area, manager, or kind site goes in
it in the same commit.

| Feature | Section | Main files | Edits go through | Tests |
|---|---|---|---|---|
| Warp mesh and grid, shape mesh | Dolls | model/warp_mesh.h, anim/shape_mesh.cpp, anim/warp.cpp, anim/grid_fold.cpp, anim/mesh_carry.cpp | RigManager (edit/rig_mesh.cpp, mesh_edits.cpp) | anim/shape_mesh_tests, anim/mesh_dots_tests, edit/mesh_edit_tests |
| Drag nodes (lag, bounce) | Dolls | anim/doll_drag.cpp, anim/drag_spring.cpp, anim/node_feel.cpp, model/doll.h | RigManager (edit/rig_nodes.cpp) | anim/drag_tests, anim/node_feel_tests, edit/rig_dots_tests |
| Cloth physics | Dolls | anim/cloth.cpp, anim/cloth_parts.cpp | RigManager | anim/cloth_tests, anim/cloth_feel_tests |
| Jelly physics, jelly knobs | Dolls | anim/jelly.cpp, anim/jelly_fit.cpp, model/node_physics.cpp, anim/cloth.cpp (ClothSettingsOf), anim/doll_drag.cpp, ui/dot_kinds.cpp (labels) | RigManager | anim/jelly_tests, anim/node_feel_tests, io/doll_file_tests |
| Feels and presets for dots | Dolls | anim/node_feel.cpp, anim/mirror.cpp | PresetManager, RigManager (SetFeelAll) | anim/node_feel_tests, edit/preset_tests |
| Point motions (wave, pulse...) | Dolls | anim/warp_motion.cpp, io/point_motion_json.cpp, ui/point_motion_box.cpp, ui/motion_tile.cpp, ui/motion_art.cpp | RigManager (edit/rig_dots.cpp) | anim/warp_motion_tests, edit/rig_dots_tests |
| Colliders | Dolls | anim/collider.cpp, anim/doll_collide.cpp, anim/doll_colliders.cpp, io/collider_json.cpp | RigManager (edit/rig_collider.cpp) | anim/collider_tests, anim/collider_push_tests, ui/collider_ui_tests |
| Poke (hold P, drag) | Poking | anim/poke_layer.cpp, edit/poke_manager.cpp, ui/stage_poke.cpp | PokeManager (never in history) | anim/poke_layer_tests, edit/poke_manager_tests, ui/stage_poke_tests |
| Joints, rig, IK, lean, swivel | Dolls | anim/ik.cpp, anim/doll_pose.cpp, anim/doll_lean.cpp, ui/rig_canvas.cpp | RigManager, PoseManager (IK drags) | anim/pose_tests, anim/lean_tests, edit/rig_tests |
| Rig canvas rope, pivot snap | Dolls | ui/rig_canvas_hang.cpp, ui/rig_canvas_snap.cpp, ui/rig_canvas_input.cpp | RigManager | ui/rig_ui_tests |
| Mirror dot setup | Dolls | anim/mirror.cpp, edit/rig_mirror.cpp | RigManager | edit/rig_dots_tests |
| Poses, copy, paste, saved poses | Dolls | model/pose.h, edit/pose_clipboard.cpp, edit/saved_poses.cpp, io/pose_file.cpp | PoseManager, PresetManager | edit/posing_tests, anim/pose_memo_tests, edit/preset_tests |
| Keys, easing, sampling | Time | model/key.h, anim/sampler.cpp, edit/key_manager.cpp | KeyManager | anim/sampler_tests, edit/key_tests |
| Timeline rows and view | Time | edit/timeline_rows.cpp, ui/timeline_view.cpp, ui/timeline_paint.cpp, ui/timeline_chips.cpp, ui/key_chip_bar.cpp, ui/step_mode_switch.cpp | KeyManager, SelectionManager | edit/timeline_tests, ui/timeline_view_tests |
| Timing chart, spacing | Time | anim/timing_chart.cpp, edit/timing_chart_manager.cpp | TimingChartManager | anim/timing_chart_tests, edit/timing_chart_edit_tests |
| Shots, master track | Shots | model/shot.h, anim/master_timeline.cpp, ui/shot_strip.cpp | ShotManager (edit/shot_manager.cpp) | edit/shot_tests, ui/shot_strip_tests |
| Stage contents, layers, effects | Shots | model/layer.h, edit/stage_manager.cpp, render/effects.cpp | StageManager | edit/stage_tests, render/render_tests |
| Links (follow another's move) | Links | anim/links.cpp, edit/link_manager.cpp, ui/link_box.cpp | LinkManager | anim/link_tests, edit/link_manager_tests |
| Reel, clips, cuts, markers | The reel | model/reel.h, anim/reel_timeline.cpp, edit/reel_edits.cpp, ui/reel_timeline.cpp | ReelManager | anim/reel_tests, edit/reel_editing_tests, ui/reel_timeline_tests |
| Reel transitions | The reel | model/transition.h, edit/reel_transitions.cpp, ui/transition_picker.cpp | ReelManager | edit/reel_transition_tests, render/render_tests |
| Rendering (stage, warp, preview) | Rendering | render/frame_renderer.cpp, render/layer_painter.cpp, render/warp_raster.cpp, render/warp_cache.cpp | none (draws only) | render/render_tests, render/warp_raster_tests, render/warp_cache_tests |
| Save, load, JSON, legacy files | Project data and undo | io/project_file.cpp, io/doll_json.cpp, io/json_values.cpp, io/reconcile.cpp | DocumentManager | io/project_file_tests, io/doll_file_tests, io/warp_json_tests |
| Picking and multi-select | Picking many | edit/selection_manager.cpp, render/stage_hit.cpp, ui/picked_dot.cpp | SelectionManager | edit/selection_tests, ui/multi_dot_tests |
| Undo, redo, history, drag steps | The one edit path | edit/history_manager.cpp, edit/edit_scope.cpp, edit/project_edits.cpp | HistoryManager | edit/history_tests |
| Stage view, handles, grab | Managers | ui/stage_view.cpp, ui/stage_view_camera.cpp, render/stage_camera.cpp, anim/guides.cpp, ui/pose_tool_guides.cpp, ui/pose_tool_nudge.cpp, edit/selection_step.cpp, render/stage_geometry.cpp, ui/grab.cpp, ui/pose_tool.cpp | PoseManager, RigManager | ui/pose_tool_tests, ui/grab_tests |
| Dot groups (pick a set at once, default per doll) | Picking many | model/dot_group.h, edit/group_edits.cpp, edit/group_manager.cpp, edit/group_mirror.cpp, edit/selection_prune.cpp (PickGroup), ui/group_box.cpp, ui/group_chips.cpp, io/dot_group_json.cpp | GroupManager | edit/group_manager_tests, io/dot_group_json_tests, ui/group_box_tests, ui/group_chips_tests |
| Cast cards, drag a doll to the stage | Picking many | ui/doll_cards.cpp, ui/doll_drop.cpp, ui/cast_actions.cpp, ui/stage_view_drop.cpp, ui/shot_thumbs.cpp, ui/shot_strip_paint.cpp, ui/reel_timeline_badges.cpp | StageManager, DollLibraryManager | ui/cast_panel_tests |
| Find a command, shortcuts, burst, sounds | Managers | ui/command_palette.cpp, ui/command_search.cpp, ui/shortcut_sheet.cpp, ui/success_burst.cpp, ui/ui_sound.cpp, ui/blip_wave.cpp, ui/choice_row.cpp, ui/new_project_dialog.cpp | the managers the actions call | ui/window_tests, ui/refusal_reason_tests |
| Panels and inspector | Picking many | ui/inspector.cpp, ui/layer_box.cpp, ui/drag_box.cpp, ui/rig_panel.cpp | the manager the panel calls | ui/inspector_tests, ui/drag_panel_tests, ui/dot_kinds_tests |
| Feel preview (hopping blob) | Picking many | anim/hop_preview.cpp, ui/wobble_blob.cpp, ui/drag_box_show.cpp | HopPreview (no edits) | anim/hop_preview_tests, ui/drag_panel_tests |
| Playback, song, export | Time | edit/playback_manager.cpp, edit/export_manager.cpp, media/video_encoder.cpp | PlaybackManager, ExportManager | edit/playback_tests, edit/export_tests, media/media_tests |
| Krita exporter | Dolls | tools/krita/snapper_doll_export/ | none (writes doll folders) | io/doll_file_tests |

## Adding a new kind

Most kind enums have a `k...Count` constant used in asserts, a
`switch` with no `default`, and a names array in `io` for JSON. Build
with warnings on: a missed `switch` case warns. Bump the Count, then
search the enum name across `src` and `tests`. Order in JSON name
arrays must match enum order.

NodeKind (model/doll.h, a new dot material):
- model/doll.h: enum, `kNodeKindCount`, DragNode fields if needed
- anim/node_feel.cpp: two switches (default feel, labels or ranges)
- anim/cloth.cpp ClothSettingsOf: `== kJelly` picks the jelly knobs
  (model/node_physics.h), anything else reads the cloth numbers
- anim/doll_drag.cpp, anim/cloth.cpp or a new solver: the physics
- anim/hop_preview.cpp: `== kDrag` (spring) and `== kJelly`, else
  cloth: the preview runs the new solver too
- anim/mesh_carry.cpp: carries physics nodes with the mesh
- io/doll_json.cpp: save and load (kind read by name, count assert)
- edit/rig_nodes.cpp: range assert; edit/rig_manager.h SetNodeKindAll
- render/stage_geometry.cpp: `kinds` for the stage
- ui/dot_shape.cpp: shape; ui/dot_kinds.cpp: Material mapping, three
  switches; also ui/dot_kinds.h `Material`, `kMaterialCount`
- ui/kind_tiles.cpp, ui/kind_tile.cpp, ui/material_art.cpp,
  ui/wobble_blob.cpp, ui/drag_box.cpp: tile, art, preview, panel
- tests: anim/node_feel_tests, anim/<solver>_tests, io/doll_file_tests,
  ui/dot_shape_tests, ui/dot_kinds_tests, ui/drag_panel_tests,
  edit/rig_dots_tests

Compare-against-one-value sites (no switch, silent when a kind is
added). Each treats a new kind as "physics" or "not drag". Check each:
- anim/doll_drag.cpp: `kind != kDrag` (is_physics) and `== kJelly`
  (a new kind is not jelly)
- anim/mesh_carry.cpp: `kind != kDrag`
- anim/cloth.cpp: asserts `!= kDrag`
- anim/node_feel.cpp: twice `== kDrag` (is_drag)
- io/doll_json.cpp: a nested ternary reads the name (a new name falls
  to kDrag), then writes knobs for `== kJelly` and the cloth numbers
  for any other physics kind, and loads old jelly through JellyFromOld
- ui/dot_kinds.cpp: returns kDrag as a fallback after the switch

WarpMotionKind (model/doll.h, a new point motion):
- model/doll.h: enum, `kWarpMotionKindCount`
- anim/warp_motion.cpp: switch for the wave shape; also compares to
  `kShiver`, `kNone` and `kWave`
- io/point_motion_json.cpp: `kMotionNames` array, legacy switch (old
  files), two `!= kNone`
- edit/rig_manager.cpp and edit/rig_dots.cpp: range checks and
  `kNone` compares
- ui/point_motion_box.cpp: label switch, the list, and
  `== kShiver` (a size or hint rule)
- tests: anim/warp_motion_tests, io/doll_file_tests, edit/rig_tests,
  edit/rig_dots_tests, ui/inspector_tests

TransitionKind (model/transition.h):
- model/transition.h: enum, `kTransitionKindCount`
- edit/reel_transitions.cpp: `TransitionName` switch, count assert
- io/reel_json.cpp: `kTransitionNames`; two `== kCut` compares
- render/frame_renderer.cpp: the mix switch (swipes share one case)
- ui/transition_picker.cpp: list from the Count, `kCut` compares
- model/transition.cpp and anim/reel_timeline.cpp use kCut and
  kCrossfade only (a compare, not a switch)
- tests: edit/reel_transition_tests, render/render_tests,
  io/project_file_tests, model/model_tests, ui/reel_timeline_tests

EffectKind (model/layer.h):
- model/layer.h: enum, `kEffectKindCount`
- render/effects.cpp: `ApplyEffect` switch (the look)
- edit/stage_manager.cpp: `EffectName` switch, count assert; and
  edit/stage_settings.cpp range assert
- io/layer_json.cpp: `kEffectNames`
- ui/cast_panel.cpp and ui/layer_box.cpp list from the Count
- render/layer_painter.cpp and render/stage_hit.cpp assert only
- tests: render/render_tests, edit/stage_tests, io/project_file_tests

Material (ui/dot_kinds.h, the four tiles): follows NodeKind; see its
list. It has its own `kMaterialCount` and switches in ui/dot_kinds.cpp
and ui/kind_tiles.cpp. ui/drag_box.cpp and ui/drag_box_show.cpp
compare to `kStill` only.

Other enums with switch sites:
- Ease (model/key.h): anim/sampler.cpp, io/json_values.cpp
  (`kEaseNames`), ui/timeline_input.cpp, anim/timing_chart.cpp.
- TrackKind (edit/track_ref.h): switches in edit/timeline_rows.cpp,
  pose_manager.cpp, key_clipboard.cpp, saved_poses.cpp,
  selection_manager.cpp, timing_chart_manager.cpp; ui/pose_tool.cpp,
  keyed_boxes.cpp, layer_box_commits.cpp, playhead.cpp.
- GrabKind (ui/grab.h): ui/grab.cpp cursor switch plus a range assert
  at its top; add the handle to the canvas that draws and hits it
  (ui/pose_tool_handles.cpp, rig_canvas_handles.cpp, stage_view.cpp,
  timeline_paint.cpp, reel_timeline_input.cpp); tests ui/grab_tests,
  ui/handle_hover_tests.
- GhostKind (ui/drag_ghost.h): each view's *_ghost.cpp and
  ui/drag_feedback.cpp; test ui/drag_feedback_tests.
- DotShape (ui/dot_shape.h): ui/dot_shape.cpp, ui/stage_view_paint.cpp;
  test ui/dot_shape_tests.
- MeshDetail (model/warp_mesh.h): anim/shape_mesh.cpp, io/warp_json.cpp,
  edit/mesh_edits.cpp, edit/rig_mesh.cpp, ui/rig_panel.cpp; tests
  anim/shape_mesh_tests, io/warp_json_tests, edit/mesh_edit_tests.
- ChangeEffectKind (ui/change_effects.h): ui/change_effects.cpp, all
  compares to one value; test ui/change_effects_tests.
- LayerContent (model/layer.h variant): `kLayerTypes` in
  io/layer_json.cpp is size-checked by `static_assert`.
