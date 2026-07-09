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
