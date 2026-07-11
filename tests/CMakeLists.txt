add_executable(WhatsUISmokeTests
    whatsui_smoke_tests.cpp
)

target_link_libraries(WhatsUISmokeTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUISmokeTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_smoke_tests
    COMMAND WhatsUISmokeTests
)

add_executable(WhatsUILayoutTests
    layout_tests.cpp
)

target_link_libraries(WhatsUILayoutTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUILayoutTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_layout_tests
    COMMAND WhatsUILayoutTests
)

add_executable(WhatsUIStateTests
    state_tests.cpp
)

target_link_libraries(WhatsUIStateTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIStateTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_state_tests
    COMMAND WhatsUIStateTests
)

add_executable(WhatsUIWindowTests
    window_tests.cpp
)

target_link_libraries(WhatsUIWindowTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIWindowTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_window_tests
    COMMAND WhatsUIWindowTests
)

add_executable(WhatsUIScrollViewTests
    scroll_view_tests.cpp
)

target_link_libraries(WhatsUIScrollViewTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIScrollViewTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_scroll_view_tests
    COMMAND WhatsUIScrollViewTests
)

# The Todo sample is a deterministic, headless Software-renderer walkthrough.
# Keep its pixel hashes in source control so UI changes are reviewed as visual
# changes, while leaving the WhatsCanvas package's own test registration alone.
if(TARGET WhatsUITodoApp)
    add_test(
        NAME whatsui_todo_visual_regression
        COMMAND ${CMAKE_COMMAND}
            -DWHATSUI_TODO_EXECUTABLE=$<TARGET_FILE:WhatsUITodoApp>
            -DWHATSUI_TODO_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/todo_visual_regression
            -P ${CMAKE_CURRENT_SOURCE_DIR}/VerifyTodoVisualHashes.cmake
    )
    set_tests_properties(whatsui_todo_visual_regression PROPERTIES
        LABELS "visual;software"
    )

    # Screenshot artifacts for human layout review.  This guards breakpoint
    # coverage and valid physical captures, not their pixels; use it whenever
    # modifying the Todo page's visual system.
    add_test(
        NAME whatsui_todo_visual_review
        COMMAND ${CMAKE_COMMAND}
            -DWHATSUI_TODO_EXECUTABLE=$<TARGET_FILE:WhatsUITodoApp>
            -DWHATSUI_TODO_REVIEW_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/todo_visual_review
            -P ${CMAKE_CURRENT_SOURCE_DIR}/CaptureTodoVisualReview.cmake
    )
    set_tests_properties(whatsui_todo_visual_review PROPERTIES
        LABELS "visual;software;review"
        # Software readback creates twelve independent 2x canvases.  The
        # review job is intentionally comprehensive and can exceed CTest's
        # default timeout on hosted Windows runners.
        TIMEOUT 180
    )
endif()
