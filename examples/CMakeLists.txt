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
    component_gallery/view_model/visual_qa_view_model.cpp
    component_gallery/view/app_shell_view.cpp
    component_gallery/view/gallery_page_factory.cpp
    component_gallery/view/components/component_card.cpp
    component_gallery/view/components/navigation_rail.cpp
    component_gallery/view/components/page_header.cpp
    component_gallery/view/components/preview_surface.cpp
    component_gallery/view/pages/about_page.cpp
    component_gallery/view/pages/addons_page.cpp
    component_gallery/view/pages/all_components_page.cpp
    component_gallery/view/pages/button_detail_page.cpp
    component_gallery/view/pages/controls_page.cpp
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
