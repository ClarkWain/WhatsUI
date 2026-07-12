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

# --- Interactive window example (GLFW + OpenGL) ---
if(TARGET WhatsUI::Glfw)
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
