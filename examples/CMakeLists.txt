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
)

target_link_libraries(WhatsUITodoApp
    PRIVATE
        WhatsUI::WhatsUI
        WhatsCanvas::Software
)

target_compile_features(WhatsUITodoApp PRIVATE cxx_std_17)

# --- Interactive window example (GLFW + OpenGL) ---
if(TARGET WhatsUI::Glfw)
    # Same declarative Todo tree as WhatsUITodoApp, presented by the GLFW host.
    # The compile definition only selects its interactive entry point.
    add_executable(WhatsUITodoGlfw
        todo_app/main.cpp
    )

    target_link_libraries(WhatsUITodoGlfw
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_definitions(WhatsUITodoGlfw PRIVATE WUI_TODO_INTERACTIVE=1)
    target_compile_features(WhatsUITodoGlfw PRIVATE cxx_std_17)

    add_executable(WhatsUIHelloWindow
        hello_window/main.cpp
    )

    target_link_libraries(WhatsUIHelloWindow
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_features(WhatsUIHelloWindow PRIVATE cxx_std_17)
endif()
