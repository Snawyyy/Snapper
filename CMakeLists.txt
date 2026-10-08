cmake_minimum_required(VERSION 3.24)

project(Snapper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
  Concurrent Core Gui Multimedia Widgets Test)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
  libavcodec libavformat libavutil libswresample libswscale)

include(cmake/snapper.cmake)
include(CTest)

# Modules, lowest first. See ARCHITECTURE.md for what each one owns.
snapper_module(base
  SOURCES frame.cpp text.cpp
  LIBS Qt6::Core)
snapper_module(model
  SOURCES doll.cpp layer.cpp project.cpp shot.cpp
  MODULES base
  LIBS Qt6::Gui)
snapper_module(anim
  SOURCES doll_drag.cpp doll_lean.cpp doll_pose.cpp ik.cpp jitter.cpp links.cpp
          master_timeline.cpp mirror.cpp presets.cpp
          sampler.cpp warp.cpp
  MODULES model)
snapper_module(io
  SOURCES doll_file.cpp doll_json.cpp json_file.cpp json_values.cpp
          layer_json.cpp legacy_doll.cpp pose_file.cpp project_file.cpp
          reconcile.cpp
  MODULES model)
snapper_module(media
  SOURCES audio_clip.cpp audio_player.cpp ffmpeg_handles.cpp
          video_encoder.cpp video_streams.cpp
  MODULES base
  LIBS Qt6::Gui Qt6::Multimedia PkgConfig::FFMPEG)
snapper_module(render
  SOURCES effects.cpp frame_renderer.cpp image_cache.cpp layer_painter.cpp
          stage_geometry.cpp stage_hit.cpp warp_raster.cpp
  MODULES anim)
snapper_module(edit
  SOURCES doll_library_manager.cpp document_manager.cpp edit_scope.cpp
          export_manager.cpp
          history_manager.cpp key_clipboard.cpp key_manager.cpp
          playback_manager.cpp pose_clipboard.cpp pose_manager.cpp
          preset_manager.cpp saved_poses.cpp
          project_edits.cpp rig_batch.cpp rig_manager.cpp rig_mirror.cpp
          selection_manager.cpp selection_prune.cpp shot_batch.cpp
          shot_manager.cpp stage_batch.cpp stage_manager.cpp
          stage_settings.cpp timeline_rows.cpp
  MODULES io media render
  LIBS Qt6::Concurrent)
snapper_module(ui
  SOURCES cast_actions.cpp cast_panel.cpp export_dialog.cpp
          export_dialog_actions.cpp file_menu.cpp main_window.cpp
          drag_box.cpp shot_box.cpp form_helpers.cpp inspector.cpp keyed_boxes.cpp layer_box.cpp
          layer_box_commits.cpp motion_box.cpp playhead.cpp rig_canvas.cpp
          rig_canvas_input.cpp rig_panel.cpp rig_panel_refresh.cpp new_project_dialog.cpp pose_tool.cpp pose_tool_handles.cpp
          shot_strip.cpp
          stage_view.cpp theme.cpp timeline_input.cpp timeline_paint.cpp
          timeline_view.cpp
  MODULES edit
  LIBS Qt6::Widgets)

qt_add_executable(snapper src/app/main.cpp src/app/app_context.cpp)
target_link_libraries(snapper PRIVATE snapper_ui)
snapper_warnings(snapper)
set_property(GLOBAL APPEND PROPERTY SNAPPER_LAYERS "app=ui")
target_include_directories(snapper PRIVATE src)
set_target_properties(snapper PROPERTIES WIN32_EXECUTABLE TRUE)

if(BUILD_TESTING)
  snapper_test(base SOURCES base_tests.cpp)
  snapper_test(model SOURCES model_tests.cpp)
  snapper_test(anim SOURCES sampler_tests.cpp pose_tests.cpp lean_tests.cpp
                    drag_tests.cpp link_tests.cpp)
  snapper_test(io SOURCES doll_file_tests.cpp project_file_tests.cpp)
  snapper_test(media SOURCES media_tests.cpp)
  snapper_test(render SOURCES render_tests.cpp)
  snapper_test(edit SOURCES document_tests.cpp export_tests.cpp
                            history_tests.cpp
                            key_tests.cpp library_tests.cpp posing_tests.cpp
                            playback_tests.cpp preset_tests.cpp
                            rig_tests.cpp
                            shot_tests.cpp
                            selection_tests.cpp stage_tests.cpp
                            timeline_tests.cpp track_tests.cpp)
  snapper_test(ui SOURCES cast_panel_tests.cpp export_dialog_tests.cpp
                          inspector_tests.cpp
                          pose_tool_tests.cpp rig_ui_tests.cpp
                          shot_strip_tests.cpp
                          theme_tests.cpp
                          timeline_view_tests.cpp window_tests.cpp)
  snapper_checks()
endif()
