# Feature map

Find the row for your change, read only the named `##` section of
`ARCHITECTURE.md` (Grep the heading for its line, then Read from
there), edit in the named manager, add the test in the named file.
Paths are under `src/` and `tests/`. The long sections have no
sub-headings: search them for the feature word.

Keep this file true: a new feature area, manager, or kind site goes in
it in the same commit.

| Feature | Section | Main files | Edits go through | Tests |
|---|---|---|---|---|
| Characters: vector drawing, parts tree, bones and binding, IK, switches, smart bones, Character tab, posing on the Pose tab | Characters | model/vector.h, model/character.h, anim/vector_path.cpp, anim/character_pose.cpp, anim/bone_ik.cpp, anim/stroke_fit.cpp, render/character_paint.cpp, render/stage_geometry.cpp (CharacterBonesOnScreen), io/character_json.cpp, io/character_file.cpp, edit/vector_edits.cpp, edit/character_edits.cpp, edit/character_pose_edits.cpp, edit/character_manager.cpp, edit/character_pose_manager.cpp, edit/track_ref.h (kCharacter), ui/character_page.cpp, ui/character_canvas.cpp, ui/character_canvas_input.cpp, ui/character_parts_panel.cpp, ui/character_inspector.cpp, ui/character_session.cpp, ui/character_box.cpp, ui/character_thumb.cpp, ui/part_icons.cpp, ui/bone_glyph.cpp, ui/pose_tool.cpp (MoveBone) | CharacterManager (rig), CharacterPoseManager (keys) | model/character_tests, io/character_json_tests, anim/character_pose_tests, edit/character_edit_tests |
| Character lines and looks: line brushes, named styles, Line width, Delete edge and Magnet tools | Characters | model/vector.h (LineBrush, VectorShape::brush, ::style), model/character.h (VectorStyle, LookOf), render/line_brush.cpp, render/character_paint.cpp, edit/vector_reshape.cpp, edit/character_styles.cpp, edit/character_manager_shapes.cpp, io/character_json.cpp, ui/character_canvas_tools.cpp, ui/character_styles_panel.cpp, ui/character_inspector.cpp (Look, Brush) | CharacterManager | edit/character_edit_tests, render/line_brush_tests, io/character_json_tests, ui/character_page_tests |
| Layer poses, editing many picked frames | Time | model/pose.h (LayerPose), anim/layer_transform.cpp, edit/pose_manager.cpp, edit/pose_shift.cpp (Shift, SetField on many frames), edit/selection_manager.cpp (EditFrames: the picked keys' frames), ui/keyed_boxes.cpp (typed sets, drags add) | PoseManager | edit/posing_tests |
| Keyed draw order (a layer's place among layers, the Order field, Ctrl+] and Ctrl+[) | Rendering | model/pose.h (LayerPose::order), anim/sampler.cpp (holds), anim/layer_order.cpp (LayersInDrawOrder: the one draw and pick order of a shot's layers), render/frame_renderer.cpp and render/stage_hit.cpp (walk it), edit/pose_shift.cpp (SetField, StepOrder, OrderName), ui/keyed_boxes.cpp (Order field), ui/reset_on_double_click.cpp, ui/order_menu.cpp (Edit menu keys) | PoseManager | anim/sampler_tests |
| Keys, easing, sampling | Time | model/key.h, anim/sampler.cpp, edit/key_manager.cpp | KeyManager | anim/sampler_tests, edit/key_tests |
| Timeline rows and view | Time | edit/timeline_rows.cpp, ui/timeline_view.cpp, ui/timeline_view_keys.cpp, ui/timeline_paint.cpp, ui/timeline_chips.cpp, ui/key_chip_bar.cpp, ui/step_mode_switch.cpp | KeyManager, SelectionManager | edit/timeline_tests, ui/timeline_view_tests |
| Timeline view shared by Pose, Video and Comp (zoom, scroll, fit, wheel, Ctrl+= Ctrl+- Ctrl+0, follow a playing playhead, ruler in frames, playhead tag, Scrub/Animation switch in the corner, Alt+Left/Right nudge, Show all) | Time | ui/timeline_scale.cpp (TimelineScale), ui/timeline_ruler.cpp (PaintFrameRuler, PaintTimelinePlayhead), ui/timeline_keys.cpp (ViewKey, NudgeOf), ui/step_mode_switch.cpp (CornerRect), ui/timeline_view.cpp and ui/timeline_view_keys.cpp, ui/reel_timeline.cpp and ui/reel_timeline_commands.cpp, ui/comp_timeline.cpp and ui/comp_timeline_keys.cpp | PlaybackManager (mode, playing), KeyManager::Shift, ReelManager::MoveAll, CompManager::SetTiming | none yet (owner tests by eye) |
| Timing chart, spacing | Time | anim/timing_chart.cpp, edit/timing_chart_manager.cpp | TimingChartManager | anim/timing_chart_tests, edit/timing_chart_edit_tests |
| Shots, master track | Shots | model/shot.h, anim/master_timeline.cpp, ui/shot_strip.cpp | ShotManager (edit/shot_manager.cpp) | edit/shot_tests, ui/shot_strip_tests |
| Stage contents, layers, effects | Shots | model/layer.h, edit/stage_manager.cpp, render/effects.cpp | StageManager | edit/stage_tests, render/render_tests |
| Links (follow another's move) | Links | anim/links.cpp, edit/link_manager.cpp, ui/link_box.cpp | LinkManager | anim/link_tests, edit/link_manager_tests |
| Reel, clips, cuts, markers | The reel | model/reel.h, anim/reel_timeline.cpp, edit/reel_edits.cpp, ui/reel_timeline.cpp | ReelManager | anim/reel_tests, edit/reel_editing_tests, ui/reel_timeline_tests |
| Resizing shots on the reel, fit to gap | The reel | edit/reel_resize.cpp, edit/shot_timing.cpp, ui/reel_timeline_input.cpp, ui/video_page.cpp | ReelManager | none yet (owner tests by eye) |
| Sound on the reel (sound tracks, volume line, fades, Mute and Solo, shots borrow the reel's sound, WAV export) | The reel | model/reel.h (SoundSource, ReelTrack sound fields), edit/reel_sound.cpp, edit/reel_edits.cpp (WrongTrack, FitFades), edit/sound_manager.cpp, edit/playback_manager.cpp (SoundOn, ShotsSound), edit/export_manager.cpp, edit/document_manager.cpp, io/reel_json.cpp, media/audio_clip.cpp (ProbeSoundSeconds), ui/reel_timeline_sound.cpp, ui/reel_timeline_drop.cpp, ui/timeline_view.cpp (PaintWave, PickSoundClip), ui/export_dialog_actions.cpp, ui/video_page.cpp | ReelManager, SoundManager | none yet (owner tests by eye) |
| Text and subtitles on the reel (text clips, In and Out, styles, subtitle track, tap L, SRT and WebVTT, subtitle export) | The reel | model/reel.h (TextSource, ReelTextLook, TextStyle, ReelTrack::is_subtitle, LookOf), edit/reel_text.cpp, edit/reel_subtitles.cpp, edit/reel_edits.cpp (WrongTrack), edit/export_manager.cpp (SubtitleOut, WriteSubtitles), render/reel_text.cpp, render/layer_painter.cpp (TextPath, PaintLettering), io/reel_json.cpp (ReelTextToJson), io/subtitle_file.cpp, ui/reel_preview.cpp, ui/reel_text_panel.cpp, ui/subtitle_panel.cpp, ui/inline_text_edit.cpp, ui/video_page.cpp (T+, L), ui/reel_timeline.cpp (PaintTextBody), ui/export_dialog.cpp | ReelManager, ExportManager | none yet (owner tests by eye) |
| Recording sound (arm, count-in, take from the playhead, WAV beside the project) | The reel | media/audio_recorder.cpp, edit/record_manager.cpp, ui/record_button.cpp, ui/reel_timeline_sound.cpp (R in the header, Arm entry), ui/reel_timeline_commands.cpp (Escape), ui/video_page.cpp | RecordManager (takes land through ReelManager::AddSounds) | none (owner tests by eye) |
| Voice gen (clone from a sample or PC audio, transcribe the sample with Whisper, design a voice, saved voices, preview, add at the playhead) | The reel | tools/voice_gen/voice_gen.py (the worker, copied beside the binary by CMakeLists.txt), tools/voice_gen/setup.sh, media/voice_worker.cpp (process, JSON lines), media/desktop_recorder.cpp (parec), edit/voice_gen_manager.cpp, edit/voice_library.cpp, edit/sidecar_folder.cpp (folder rule shared with takes), ui/voice_panel.cpp, ui/wave_screen.cpp, ui/led_meter.cpp, ui/hero_button.cpp, ui/bevel.cpp, ui/voice_clone_box.cpp, ui/audio_preview.cpp, ui/voice_name_prompt.cpp, ui/video_page_side.cpp (Voice tab), app/app_context.cpp | VoiceGenManager (the line, PC samples, adding through ReelManager::AddSounds), VoiceLibrary (saved voices) | none yet (owner tests by eye) |
| New shot here, open a shot from the reel | The reel | edit/reel_new_shot.cpp (AddNewShot, NewShotSpan), ui/reel_timeline_input.cpp (Alt drag mark, double click), ui/reel_timeline_commands.cpp (menus), ui/shot_bin.cpp, ui/video_page.cpp (OpenShotAsked, "+" tile) | ReelManager | none (owner tests by eye) |
| Video tab side panel and layout (only what the pick needs, Sound clip and Shot clip panels, track scroll, wrapping bar) | The reel | ui/video_page_side.cpp (SidePanelsFor, ShowSide), ui/sound_clip_panel.cpp, ui/shot_clip_panel.cpp, ui/reel_timeline.cpp (FitScrollBar), ui/reel_timeline_drop.cpp (wheel) | ReelManager, ShotManager | none (owner tests by eye) |
| Reel transitions | The reel | model/transition.h, edit/reel_transitions.cpp, ui/transition_picker.cpp | ReelManager | edit/reel_transition_tests, render/render_tests |
| Rendering (stage, preview) | Rendering | render/frame_renderer.cpp, render/layer_painter.cpp, render/image_cache.cpp | none (draws only) | render/render_tests |
| Save, load, JSON, legacy files (old doll layers left out) | Project data and undo | io/project_file.cpp, io/layer_json.cpp, io/json_values.cpp, io/character_json.cpp | DocumentManager | io/project_file_tests, io/character_json_tests |
| Picking and multi-select | Picking many | edit/selection_manager.cpp, render/stage_hit.cpp | SelectionManager | edit/selection_tests |
| One pick rule on canvases, strips and lists; references in the pick (Shift, Ctrl, Ctrl+A, Delete, Escape on reference rows) | Picking many | ui/pose_tool.cpp, ui/character_canvas_input.cpp, ui/timeline_input.cpp, ui/shot_strip.cpp, ui/timeline_references.cpp, ui/reference_keys.cpp, edit/reference_manager.cpp (PickMany, RemovePicked), app/app_context.cpp | SelectionManager, ReferenceManager | ui/pose_tool_tests |
| Cast panel tabs (Characters, Layers, Add menu with Background) and the tape player under them | Picking many | ui/cast_panel.cpp, ui/cassette_deck.cpp, ui/divider_tabs.cpp, ui/glass_door.cpp, ui/cast_actions.cpp, edit/stage_manager.cpp (AddBackground) | StageManager, CharacterManager | ui/cast_panel_tests |
| The desk: cel stack layers with eye and lock, light table, polaroid shot strip, DVD burner export, sticky note toasts | Managers | ui/cel_stack.cpp, ui/cast_actions.cpp, model/layer.h (is_locked), edit/stage_settings.cpp (SetLocked), render/stage_hit.cpp, anim/light_table.cpp, ui/stage_view_light.cpp, ui/shot_strip_paint.cpp, ui/disc_burner.cpp, ui/export_dialog.cpp, ui/toast_queue.cpp (ToastKind), ui/toast_view.cpp | StageManager | ui/cast_panel_tests, anim/light_table_tests, edit/stage_tests, render/render_tests, ui/toast_tests, ui/export_dialog_tests, ui/character_stage_tests |
| Achievements and stats (trophy case, gold pop-up) | Managers | edit/achievements.cpp (catalog, Measure), edit/achievement_manager.cpp, ui/achievement_icons.cpp, ui/achievements_window.cpp, ui/trophy_case.cpp, app/app_context.cpp | AchievementManager (never the project) | edit/achievement_tests, ui/character_stage_tests |
| Undo, redo, history, drag steps, per-shot undo | The one edit path | edit/history_manager.cpp, edit/history_scoped.cpp, edit/step_scope.cpp, edit/edit_scope.cpp, edit/project_edits.cpp | HistoryManager | edit/history_tests |
| Stage view, handles, grab | Managers | ui/stage_view.cpp, ui/stage_view_camera.cpp, ui/stage_view_flash.cpp, ui/stage_flash.cpp, render/stage_camera.cpp, anim/guides.cpp, ui/pose_tool_guides.cpp, ui/pose_tool_nudge.cpp, edit/selection_step.cpp, render/stage_geometry.cpp, ui/grab.cpp, ui/pose_tool.cpp | PoseManager, RigManager | ui/pose_tool_tests, ui/grab_tests, ui/stage_flash_tests |
| Character cards, drag a character to the stage, files dropped on the stage | Characters | ui/character_cards.cpp, ui/character_drop.cpp, ui/character_thumb.cpp, ui/cast_actions.cpp, ui/stage_view_drop.cpp, ui/shot_thumbs.cpp, ui/shot_strip_paint.cpp, ui/reel_timeline_badges.cpp | StageManager (AddCharacters), CharacterManager (Import) | ui/cast_panel_tests |
| Find a command, shortcuts, burst, sounds | Managers | ui/command_palette.cpp, ui/command_search.cpp, ui/shortcut_sheet.cpp, ui/success_burst.cpp, ui/ui_sound.cpp, ui/blip_wave.cpp, ui/choice_row.cpp, ui/new_project_dialog.cpp | the managers the actions call | ui/window_tests, ui/refusal_reason_tests |
| Panels and inspector | Picking many | ui/inspector.cpp, ui/inspector_show.cpp, edit/settings_manager.cpp, ui/settings_actions.cpp, ui/scroll_guard.cpp, ui/pose_controls.cpp, ui/fader.cpp, ui/track_pad.cpp, ui/bound_drag.cpp, ui/keyed_boxes.cpp, ui/shot_box.cpp, ui/layer_box.cpp, ui/character_box.cpp, ui/reference_box.cpp, ui/link_box.cpp, ui/storyboard_box.cpp | the manager the panel calls | ui/inspector_tests |
| Panel kit, screen fit, pick keys (plates, frames, golden rows, folds, flow layout, window and dialog fit, portrait rule, ClickMode, BoxMode, item view picking) | Picking many | ui/panel_kit.cpp, ui/fold_section.cpp, ui/form_helpers.cpp (AddTitle), ui/theme.h, ui/snapper_style.cpp, ui/snapper_accents.cpp (Teto red on picked tabs, toggled buttons, checks, focused fields, title plates), ui/snapper_style.cpp (DrawGripLine, the ridged grips and dock separators), ui/grip_splitter.cpp (GripSplitter, every page splitter: remembered sizes, double click reset, dwell light), ui/dock_grips.cpp (DockGrips: the same for the Cast and Inspector docks), ui/view_camera.cpp (wheel zoom and middle drag pan of the Comp stage and Video preview), ui/scroll_fit.cpp, ui/flow_layout.cpp, ui/screen_fit.cpp, ui/pick_keys.cpp, ui/pick_list.cpp | none (look and keys only) | none yet (owner tests by eye) |
| Playback, song, export | Time | edit/playback_manager.cpp, edit/export_manager.cpp, ui/transport_bar.cpp, ui/top_bar.cpp, ui/export_dialog.cpp, media/video_encoder.cpp, media/video_streams.cpp, media/video_formats.cpp, media/png_frames.cpp | PlaybackManager, ExportManager | edit/playback_tests, edit/export_tests, media/media_tests |
| Floating palette (color wheel, recent, the picked character's colors as a fold, shown by use, stage flash, I to pick) | Characters | edit/palette_manager.cpp (PaletteUse, ShowsCharacter, CharacterColors), render/drawing_colors.cpp, ui/palette_panel.cpp, ui/palette_panel_frame.cpp (move, resize, fit), ui/palette_eyedropper.cpp (Pick button), ui/color_wheel.cpp, ui/swatch_grid.cpp, ui/fold_section.cpp, ui/stage_view_palette.cpp | PaletteManager | none yet (owner tests by eye) |
| Editing outside the frame (pasteboard, O, Fit all, off-view arrow) | Rendering | render/frame_renderer.cpp (RenderArea), ui/stage_view_outside.cpp, ui/stage_view_camera.cpp, edit/document_manager.cpp | DocumentManager (the O switch) | none yet |
| Transparent shot background (Transparent tile, checkerboard, Shift+V, Transparent start, alpha export: PNG frames, WebM, MOV) | Rendering | model/shot.h (is_transparent), model/project.h (new_shots_transparent), io/layer_json.cpp, io/project_file.cpp, render/frame_renderer.cpp (KeepDrawnOnly, RenderReel under), media/video_formats.cpp, media/video_encoder.cpp, media/video_streams.cpp, media/png_frames.cpp, media/wav_file.cpp, edit/shot_batch.cpp, edit/export_manager.cpp, edit/document_manager.cpp, ui/shot_box.cpp, ui/checkerboard.cpp, ui/stage_view_look.cpp (PaintBehind, SolidKey), ui/shot_strip_paint.cpp, ui/reel_timeline.cpp, ui/export_dialog.cpp, ui/export_dialog_actions.cpp, ui/new_project_dialog.cpp | ShotManager (SetTransparentAll), ExportManager | none yet |
| Reference on the Pose tab (Add reference, Alt + drop, reference rows, opacity, in front, tint, fit, start and slide, speed, markers, Y, M, find file) | Shots | model/reference.h, model/shot.h (references), io/reference_json.cpp, io/layer_json.cpp, edit/reference_manager.cpp, edit/video_file_frames.cpp (PictureAtSeconds), render/reference_paint.cpp, render/frame_renderer.cpp (under), ui/stage_view_reference.cpp, ui/stage_view_drop.cpp, ui/timeline_references.cpp, ui/reference_box.cpp, ui/reference_keys.cpp, ui/shot_strip.cpp (menu), ui/inspector.cpp | ReferenceManager | none yet |
| Compositing (Comp tab: effect library and search, effect folds per pick, stage picking, targets, picked colors, blend modes, keyed knobs, shot Light, V, K, I) | Shots | model/comp.h, model/shot.h (comp, light), anim/comp_knobs.cpp (KnobSpecOf, KnobAt, LightAt), anim/comp_fsp.cpp (F's Plugins effects: slot specs, names, FspValuesAt), anim/reel_timeline.cpp (ShotAtReel: the shot Comp edits at the reel playhead), io/comp_json.cpp, io/comp_fsp_json.cpp (plugin values by parameter name, missing plugins kept), io/layer_json.cpp, render/comp.cpp (ApplyComp, CompReport), render/comp_targets.cpp (NeedsOf, CompSpace), render/comp_plugin.cpp (RunCompPlugin), render/comp_masks.cpp, render/comp_effects.cpp, render/comp_light.cpp, render/comp_smoother.cpp and render/comp_smoother_runs.cpp (Line smoother, OLM Smoother v2 rebuild), fsp/olm_smoother_info.cpp (its parameters), render/blend.cpp (BlendOnto, StandInOnto), render/frame_renderer.cpp (PaintForComp, SetCompShow), edit/comp_manager.cpp, edit/comp_edits.cpp, edit/comp_rows.cpp (RowsOf, IsKnobShown, KnobLabel, IsKeyable), edit/comp_plugins.cpp (EffectName, AddPlugin, SetKnobColor), edit/comp_view.cpp (pick, V, K, NoteCosts, WhyNotRun), edit/comp_pick.cpp (TargetFor, ShownFor, TargetText: what the stage pick means), edit/comp_clip.cpp (AddEntry, Copy, Paste), edit/comp_cut.cpp (SplitAll, CutAll, PasteAt, DuplicateAll: the Video tab's clip edits on effect clips), edit/comp_timing.cpp (SetTiming: effect clips), model/comp.cpp (SpanOf, IsLiveAt), anim/comp_reel.cpp (FirstClipOf, EffectOnReel, ScreenEffectsAt, EffectFrameAt), render/comp_reel.cpp (screen effects over the reel picture), render/frame_reel.cpp (RenderReel, DrawClip, transitions), ui/comp_timeline_bars.cpp (effect clips, snapping, drags), ui/comp_timeline_input.cpp (pick, box, wheel), ui/comp_timeline_keys.cpp (keys, menu, nudge, fit, remove), edit/comp_clip_picks.cpp (ClipPicks, PickClips), ui/comp_page.cpp (keys), ui/comp_library.cpp (every effect, search, hover help, double click or drag adds), ui/comp_drag.cpp (drag data, kind marks), ui/comp_effects.cpp (one fold per effect on the pick), ui/comp_effects_drop.cpp (plate clicks and drags, drops, menus, paste), ui/on_switch.cpp (the on square), ui/comp_light.cpp (shot Light), ui/comp_stage.cpp, ui/comp_stage_picks.cpp (red outlines, drops on a layer), ui/comp_panel.cpp (one effect's controls), ui/comp_panel_knobs.cpp (rows from RowsOf), ui/comp_timeline.cpp, ui/main_window.cpp (Mode::kComp), ui/top_bar.cpp | CompManager | tests/ui/comp_reel_tests.cpp (the reel playhead part only) |
| Storyboard layer (a kind of layer: raster cels over a shot, dashed rows on the Pose timeline, Alt+N new blank frame, Alt+Shift+N draw over, onion, opacity, In export for an animatic, eye and draw order from the layer list) | Shots | model/storyboard.h, model/layer.h (StoryboardLayer, IsStagePicture), io/storyboard_json.cpp, io/layer_json.cpp, render/storyboard_paint.cpp, render/frame_renderer.cpp (PaintBoardLayer, SetStoryboardShown), render/layer_painter.cpp, render/stage_hit.cpp, edit/storyboard_edits.cpp, edit/storyboard_manager.cpp, edit/storyboard_manager_edits.cpp, edit/stage_manager.cpp (AddStoryboard), edit/draw_manager.cpp and edit/draw_job.cpp (DrawJob), edit/timeline_rows.cpp (no row, PoseChanges), ui/storyboard_actions.cpp, ui/storyboard_menu.cpp (Edit menu, Alt+N keys), edit/storyboard_folder.cpp, io/storyboard_paths.cpp (relative paths in the file), edit/document_manager.cpp (CarryStoryboard on Save As), edit/history_manager.cpp (MapProjects), ui/storyboard_box.cpp, ui/inspector.cpp (the box), ui/timeline_storyboard.cpp and ui/timeline_storyboard_input.cpp, ui/timeline_eye.cpp, ui/timeline_references.cpp (RowsTop), ui/cast_panel.cpp (Add > Storyboard), ui/draw_canvas_paint.cpp, ui/reel_preview.cpp, ui/stage_view_draw.cpp | StoryboardManager (cels, opacity, export), StageManager (the layer) | edit/storyboard_tests, render/storyboard_render_tests, ui/storyboard_menu_tests |

## Adding a new kind

Most kind enums have a `k...Count` constant used in asserts, a
`switch` with no `default`, and a names array in `io` for JSON. Build
with warnings on: a missed `switch` case warns. Bump the Count, then
search the enum name across `src` and `tests`. Order in JSON name
arrays must match enum order.

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

CompKind (model/comp.h, a Comp tab effect):
- model/comp.h: enum, `kCompKindCount`
- anim/comp_knobs.cpp: `KnobSpecOf` switch (its knobs, labels, ranges)
- io/comp_json.cpp: `kKindNames`
- edit/comp_manager.cpp: `kKindNames`, `MadeOf` and `KindHelp`
  switches, `StyleNames`, `UsesColor`
- edit/comp_rows.cpp: `IsKnobShown` (per-kind rules), `RowsOf`
- render/comp.cpp: the `Draw` switch (the look);
  render/comp_targets.cpp `NeedsCompShape`
- ui/comp_drag.cpp: `KindMark` switch (the color mark)
- ui/comp_library.cpp: the library lists from the Count (skipping
  kFsp, whose plugins are listed by name under F's Plugins); a new
  plugin gets its line in edit/comp_plugins.cpp `kPluginHelp`
- kFsp also compares in anim/comp_fsp.cpp `FspInfoOf`,
  edit/comp_plugins.cpp `EffectName`/`EffectHelp`, edit/comp_view.cpp
  `WhyNotRun`, io/comp_json.cpp (plugin), render/comp.cpp `Run` (a
  missing plugin draws nothing)
- a kind with value slots (kFsp, kLineSmoother) also compares in
  anim/comp_fsp.cpp `ParamInfoOf` (its parameter table),
  edit/comp_rows.cpp `IsKnobShown`/`RowsOf`/`IsPluginKnobShown` and
  io/comp_fsp_json.cpp `HasParamsInFile`/`UsableInfo` (saved as
  "params")
- tests: none yet

The Line smoother (kLineSmoother) is a rebuild of OLM Smoother v2 from
its manual: its parameter table is fsp/olm_smoother_info.cpp (shaped
like a plugin's, not in `AllFspEffects`), the pixel work
render/comp_smoother.cpp (light spaces, color key, the mix) and
render/comp_smoother_runs.cpp (the MLAA edge search), run by
render/comp.cpp `DrawSmoother` and laid down by render/blend.cpp
`StandInOnto`; which rows show is edit/comp_rows.cpp
`IsSmootherSlotShown`. tools/smoother_render.cpp (target
smoother_render, not built by default) runs it on a PNG.

A new F's plugin needs nothing here: it appears from `AllFspEffects()`
(src/fsp) with its own parameters.

BlendMode (model/comp.h): `kBlendModeCount`; render/blend.cpp
`Separable` switch (or `NonSeparable`); io/comp_json.cpp
`kBlendNames`; edit/comp_manager.cpp `kBlendNames`; ui/comp_panel.cpp
`kBlendGroups` (index ranges per group).

CompKnob (model/comp.h): a fixed knob goes before `kParam0` and raises
`kCompFixedKnobCount`; io/comp_json.cpp `kKnobNames`;
anim/comp_knobs.cpp per-kind switches; points (kAx to kBy) are
compared in edit/comp_rows.cpp `IsKnobShown`. `kParam0` on are a
plugin's (or the Line smoother's) slots (`ParamKnob`, `ParamSlotOf`,
`kCompParamSlots`).

GradientShape (model/comp.h): `kGradientShapeCount`; io/comp_json.cpp
`kShapeNames`; edit/comp_manager.cpp `kShapeNames`; render/comp_light.cpp
`RampAt` switch and the four-corner and from-edge paths.

TextMotion (model/reel.h, how a text clip comes in and goes out):
- model/reel.h: enum, `kTextMotionCount`
- render/reel_text.cpp: the switch in `PaintReelText` (the look)
- io/reel_json.cpp: `kMotionNames`
- ui/reel_text_panel.cpp: `kMotionNames` (tile labels, in enum order)
- tests: none yet

TextSpot (edit/reel_manager.h, a place tile): `kTextSpotCount`;
edit/reel_text.cpp `SpotPlace` switch; ui/reel_text_panel.cpp
`kSpotNames`.

ClipSource (model/reel.h, a new clip source): every `get_if` and
`holds_alternative` on it: anim/reel_timeline.cpp (`SourceLength`),
render/frame_renderer.cpp (`DrawClip`), io/reel_json.cpp
(`ClipToJson`, `ClipFromJson`), edit/reel_edits.cpp (`WrongTrack`),
ui/reel_timeline.cpp (`PaintPictureBody`), edit/sound_manager.cpp.

VideoFormat (media/video_formats.h, an export format):
- media/video_formats.h: enum, `kVideoFormatCount`
- media/video_formats.cpp: `KeepsAlpha`, `HasSound`, `FormatSuffix`,
  `FormatName`, `WhyNoEncoder` (switches)
- media/video_encoder.cpp: the muxer name in `Open`
- media/video_streams.cpp: `PickVideoCodec`, `PixelsFor`,
  `PickAudioCodec`, `SetQuality`
- ui/export_dialog.cpp: one `format_` item per format, in enum order
- tests: edit/export_tests, media/media_tests

Other enums with switch sites:
- LineBrush (model/vector.h, `kLineBrushCount`): render/line_brush.cpp
  (`DabOf` switch, `Spacing`), io/character_json.cpp (`kBrushNames`),
  ui/character_inspector.cpp (`kBrushNames` labels in enum order); test
  render/line_brush_tests.
- CharacterTool (ui/character_session.h, `kCharacterToolCount`):
  `kTools` in ui/character_page.cpp (enum order), its icon in
  ui/tool_icons.cpp, the press switch in
  ui/character_canvas_input.cpp, the tool's own code in
  ui/character_canvas_tools.cpp.
- ToastKind (ui/toast_queue.h): `PaperOf` in ui/toast_view.cpp, the
  push in each ToastManager method.
- AchievementStat (edit/achievements.h, `kAchievementStatCount`):
  `Measure` and `IsTallied` in edit/achievements.cpp, the numbers line
  in ui/achievements_window.cpp; a stat only the UI sees is counted in
  ui/trophy_case.cpp.
- AchievementGroup (edit/achievements.h, `kAchievementGroupCount`):
  `SpriteOf` in ui/achievement_icons.cpp, `GroupName` in
  ui/achievements_window.cpp.
- PoseField (edit/pose_manager.h): `SetPoseField` switch in
  edit/pose_shift.cpp; ui/keyed_boxes.cpp runs its fields in this order
  (`PoseBox::Field`, its names array and the `was` and `shown` arrays).
- Ease (model/key.h): anim/sampler.cpp, io/json_values.cpp
  (`kEaseNames`), ui/timeline_input.cpp, anim/timing_chart.cpp.
- TrackKind (edit/track_ref.h): switches in edit/timeline_rows.cpp,
  pose_manager.cpp, key_clipboard.cpp, selection_manager.cpp,
  timing_chart_manager.cpp; ui/pose_tool.cpp,
  keyed_boxes.cpp, layer_box_commits.cpp, playhead.cpp.
- GrabKind (ui/grab.h): ui/grab.cpp cursor switch plus a range assert
  at its top; add the handle to the canvas that draws and hits it
  (ui/pose_tool_handles.cpp, character_canvas.cpp, stage_view.cpp,
  timeline_paint.cpp, reel_timeline_input.cpp); tests ui/grab_tests,
  ui/handle_hover_tests.
- GhostKind (ui/drag_ghost.h): each view's *_ghost.cpp and
  ui/drag_feedback.cpp; test ui/drag_feedback_tests.
- ChangeEffectKind (ui/change_effects.h): ui/change_effects.cpp, all
  compares to one value; test ui/change_effects_tests.
- LayerContent (model/layer.h variant): `kLayerTypes` in
  io/layer_json.cpp is size-checked by `static_assert`. A new
  alternative also needs: `Painter::operator()` in
  render/layer_painter.cpp and `LayerHitter::operator()` in
  render/stage_hit.cpp (both are exhaustive visits), `LayerKind` in
  ui/cast_actions.cpp, `IsStagePicture` in model/layer.cpp (false when
  a click or a Comp effect has nothing to work on), and the compares
  against one kind (`EffectLayer`, `StoryboardLayer`) in
  edit/timeline_rows.cpp and ui/inspector_show.cpp.
