add_executable(WhatsUIRenderToImage
    render_to_image/main.cpp
)

target_link_libraries(WhatsUIRenderToImage
    PRIVATE
        WhatsUI::WhatsUI
        WhatsCanvas::Software
)

target_compile_features(WhatsUIRenderToImage PRIVATE cxx_std_17)

add_executable(WhatsUITodoApp
    todo_app/main.cpp
    todo_app/todo_controller.cpp
    todo_app/todo_interaction.cpp
    todo_app/todo_storage.cpp
)

target_link_libraries(WhatsUITodoApp
    PRIVATE
        WhatsUI::WhatsUI
        WhatsCanvas::Software
)

target_compile_features(WhatsUITodoApp PRIVATE cxx_std_17)

set(WHATSUI_FOCUS_TOMATO_SOURCES
    focus_tomato/application/focus_data_service.cpp
    focus_tomato/domain/clock_drift.cpp
    focus_tomato/domain/focus_data.cpp
    focus_tomato/domain/focus_data_validator.cpp
    focus_tomato/domain/focus_statistics.cpp
    focus_tomato/infrastructure/file_focus_repository.cpp
    focus_tomato/presentation/focus_assets.cpp
    focus_tomato/presentation/focus_router.cpp
    focus_tomato/presentation/focus_view_model.cpp
    focus_tomato/presentation/components/common_components.cpp
    focus_tomato/presentation/dialogs/new_task_dialog.cpp
    focus_tomato/presentation/pages/completion_page.cpp
    focus_tomato/presentation/pages/focus_timer_page.cpp
    focus_tomato/presentation/pages/session_setup_page.cpp
    focus_tomato/presentation/pages/short_break_page.cpp
    focus_tomato/presentation/pages/task_list_page.cpp
)

add_library(WhatsUIFocusTomatoCore STATIC ${WHATSUI_FOCUS_TOMATO_SOURCES})
target_link_libraries(WhatsUIFocusTomatoCore PUBLIC WhatsUI::WhatsUI)
target_include_directories(WhatsUIFocusTomatoCore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/focus_tomato
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party/WhatsCanvas/third_party/stb)
target_compile_definitions(WhatsUIFocusTomatoCore
    PUBLIC FOCUS_TOMATO_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/focus_tomato")
target_compile_features(WhatsUIFocusTomatoCore PUBLIC cxx_std_17)

add_executable(WhatsUIFocusTomatoCapture focus_tomato/capture_main.cpp)
target_link_libraries(WhatsUIFocusTomatoCapture
    PRIVATE WhatsUIFocusTomatoCore WhatsCanvas::Software)
target_compile_features(WhatsUIFocusTomatoCapture PRIVATE cxx_std_17)

# The Settings reference is deliberately built from the same source for the
# deterministic Software capture and the interactive GLFW host.
add_executable(WhatsUISettingsApp
    settings_panel/main.cpp
)

target_link_libraries(WhatsUISettingsApp
    PRIVATE
        WhatsUI::WhatsUI
        WhatsCanvas::Software
)

target_compile_features(WhatsUISettingsApp PRIVATE cxx_std_17)

# Searchable command-palette reference: deterministic Software capture plus
# an interactive GLFW target from one source tree.
add_executable(WhatsUICommandPaletteApp
    command_palette/main.cpp
)

target_link_libraries(WhatsUICommandPaletteApp
    PRIVATE
        WhatsUI::WhatsUI
        WhatsCanvas::Software
)

target_compile_features(WhatsUICommandPaletteApp PRIVATE cxx_std_17)

add_executable(WhatsUIDebugInspectorApp
    debug_inspector/main.cpp
)

target_link_libraries(WhatsUIDebugInspectorApp
    PRIVATE
        WhatsUI::WhatsUI
        WhatsCanvas::Software
)

target_compile_features(WhatsUIDebugInspectorApp PRIVATE cxx_std_17)

set(WHATSUI_COMPONENT_GALLERY_SOURCES
    component_gallery/application/gallery_application.cpp
    component_gallery/application/gallery_router.cpp
    component_gallery/application/platform_url_opener.cpp
    component_gallery/domain/component_catalog.cpp
    component_gallery/domain/component_descriptor.cpp
    component_gallery/view_model/button_detail_view_model.cpp
    component_gallery/view_model/gallery_view_model.cpp
    component_gallery/view_model/gallery_view_models.cpp
    component_gallery/view_model/navigation_view_model.cpp
    component_gallery/view_model/theme_studio_view_model.cpp
    component_gallery/view_model/visual_qa_view_model.cpp
    component_gallery/view/app_shell_view.cpp
    component_gallery/view/gallery_page_factory.cpp
    component_gallery/view/components/component_card.cpp
    component_gallery/view/components/compact_navigation.cpp
    component_gallery/view/components/navigation_rail.cpp
    component_gallery/view/components/page_header.cpp
    component_gallery/view/components/preview_surface.cpp
    component_gallery/view/components/responsive_gallery_shell.cpp
    component_gallery/view/components/responsive_column_pair.cpp
    component_gallery/view/components/responsive_overview_hero.cpp
    component_gallery/view/pages/about_page.cpp
    component_gallery/view/pages/addons_page.cpp
    component_gallery/view/pages/all_components_page.cpp
    component_gallery/view/pages/button_detail_page.cpp
    component_gallery/view/pages/controls_page.cpp
    component_gallery/view/pages/long_text_page.cpp
    component_gallery/view/pages/overview_page.cpp
    component_gallery/view/pages/visual_qa_page.cpp
)

add_library(WhatsUIComponentGalleryCore STATIC ${WHATSUI_COMPONENT_GALLERY_SOURCES})
target_link_libraries(WhatsUIComponentGalleryCore PUBLIC WhatsUI::WhatsUI)
target_include_directories(WhatsUIComponentGalleryCore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/component_gallery)
target_compile_features(WhatsUIComponentGalleryCore PUBLIC cxx_std_17)
if(WIN32)
    target_link_libraries(WhatsUIComponentGalleryCore PUBLIC Shell32)
endif()

add_executable(WhatsUIComponentGalleryApp
    component_gallery/capture_main.cpp
    component_gallery/capture/headless_platform.cpp
)
target_link_libraries(WhatsUIComponentGalleryApp
    PRIVATE WhatsUIComponentGalleryCore WhatsCanvas::Software)
target_compile_features(WhatsUIComponentGalleryApp PRIVATE cxx_std_17)

# --- Interactive window example (GLFW + OpenGL) ---
if(TARGET WhatsUI::Glfw)
    add_executable(WhatsUIFocusTomatoApp focus_tomato/main.cpp)
    target_link_libraries(WhatsUIFocusTomatoApp
        PRIVATE WhatsUIFocusTomatoCore WhatsUI::Glfw)
    target_compile_features(WhatsUIFocusTomatoApp PRIVATE cxx_std_17)

    add_executable(WhatsUIComponentGalleryGlfw component_gallery/main.cpp)
    target_link_libraries(WhatsUIComponentGalleryGlfw
        PRIVATE WhatsUIComponentGalleryCore WhatsUI::Glfw)
    target_compile_features(WhatsUIComponentGalleryGlfw PRIVATE cxx_std_17)

    # Same declarative Todo tree as WhatsUITodoApp, presented by the GLFW host.
    # The compile definition only selects its interactive entry point.
    add_executable(WhatsUITodoGlfw
        todo_app/main.cpp
        todo_app/todo_controller.cpp
        todo_app/todo_interaction.cpp
        todo_app/todo_storage.cpp
    )

    target_link_libraries(WhatsUITodoGlfw
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_definitions(WhatsUITodoGlfw PRIVATE WUI_TODO_INTERACTIVE=1)
    target_compile_features(WhatsUITodoGlfw PRIVATE cxx_std_17)

    add_executable(WhatsUISettingsGlfw
        settings_panel/main.cpp
    )

    target_link_libraries(WhatsUISettingsGlfw
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_definitions(WhatsUISettingsGlfw PRIVATE WUI_SETTINGS_INTERACTIVE=1)
    target_compile_features(WhatsUISettingsGlfw PRIVATE cxx_std_17)

    add_executable(WhatsUICommandPaletteGlfw
        command_palette/main.cpp
    )

    target_link_libraries(WhatsUICommandPaletteGlfw
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_definitions(WhatsUICommandPaletteGlfw PRIVATE WUI_COMMAND_PALETTE_INTERACTIVE=1)
    target_compile_features(WhatsUICommandPaletteGlfw PRIVATE cxx_std_17)

    add_executable(WhatsUIDebugInspectorGlfw
        debug_inspector/main.cpp
    )

    target_link_libraries(WhatsUIDebugInspectorGlfw
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_definitions(WhatsUIDebugInspectorGlfw PRIVATE WUI_DEBUG_INSPECTOR_INTERACTIVE=1)
    target_compile_features(WhatsUIDebugInspectorGlfw PRIVATE cxx_std_17)

    add_executable(WhatsUIHelloWindow
        hello_window/main.cpp
    )

    target_link_libraries(WhatsUIHelloWindow
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_features(WhatsUIHelloWindow PRIVATE cxx_std_17)
endif()
