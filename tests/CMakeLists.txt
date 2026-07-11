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

if(WHATSUI_ENABLE_SANITIZERS)
    foreach(whatsui_test_target IN ITEMS
            WhatsUISmokeTests
            WhatsUILayoutTests
            WhatsUIStateTests
            WhatsUIWindowTests
            WhatsUIScrollViewTests)
        whatsui_prepare_sanitized_executable(${whatsui_test_target})
    endforeach()
endif()

# The Settings reference has a deliberately smaller contract than Todo's
# scripted product walkthrough: prove that its shared Software tree captures
# at the documented Windows desktop size. Human review owns visual direction;
# this test prevents the reference target or capture entry point from silently
# dropping out of the WhatsCanvas build.
if(TARGET WhatsUISettingsApp)
    add_test(
        NAME whatsui_settings_reference_capture
        COMMAND ${CMAKE_COMMAND}
            -DWHATSUI_SETTINGS_EXECUTABLE=$<TARGET_FILE:WhatsUISettingsApp>
            -DWHATSUI_SETTINGS_OUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/settings_reference_capture
            -P ${CMAKE_CURRENT_SOURCE_DIR}/VerifySettingsReference.cmake
    )
    set_tests_properties(whatsui_settings_reference_capture PROPERTIES
        LABELS "visual;software;settings"
        TIMEOUT 90
    )
endif()

add_test(
    NAME whatsui_scroll_view_tests
    COMMAND WhatsUIScrollViewTests
)

# Exercise the public WhatsCanvas bridge rather than WhatsCanvas internals.
# This is deliberately Windows-font-aware: it proves the default fallback
# chain and Segoe UI Emoji policy when that system font is installed, while
# still allowing a reduced CI image to validate UTF-8/bidi/line layout.
if(TARGET WhatsCanvasSoftware)
    add_executable(WhatsUITextShapingTests
        text_shaping_tests.cpp
    )
    target_link_libraries(WhatsUITextShapingTests
        PRIVATE
            WhatsUI::WhatsUI
            WhatsCanvas::Software
    )
    target_compile_features(WhatsUITextShapingTests PRIVATE cxx_std_17)
    add_test(
        NAME whatsui_text_shaping_tests
        COMMAND WhatsUITextShapingTests
    )
    set_tests_properties(whatsui_text_shaping_tests PROPERTIES LABELS "text;unicode;software")

    add_executable(WhatsUITextInputCompositionVisualTests
        text_input_composition_visual_tests.cpp
    )
    target_link_libraries(WhatsUITextInputCompositionVisualTests
        PRIVATE
            WhatsUI::WhatsUI
            WhatsCanvas::Software
    )
    target_compile_features(WhatsUITextInputCompositionVisualTests PRIVATE cxx_std_17)
    add_test(
        NAME whatsui_text_input_composition_visual_tests
        COMMAND WhatsUITextInputCompositionVisualTests
    )
    set_tests_properties(whatsui_text_input_composition_visual_tests PROPERTIES LABELS "text;ime;visual;software")
endif()

add_executable(WhatsUILifecycleTests
    lifecycle_tests.cpp
)

target_link_libraries(WhatsUILifecycleTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUILifecycleTests PRIVATE cxx_std_17)

if(WHATSUI_ENABLE_SANITIZERS)
    whatsui_prepare_sanitized_executable(WhatsUILifecycleTests)
endif()

add_test(
    NAME whatsui_lifecycle_tests
    COMMAND WhatsUILifecycleTests
)

# When WhatsUI is configured with its optional renderer bridge, widget object
# files reference WhatsCanvas symbols even for otherwise headless runtime
# tests. Link that backend consistently so the Windows test executables remain
# runnable in both supported build configurations.
if(TARGET WhatsCanvas::Software)
    foreach(whatsui_runtime_test_target IN ITEMS
            WhatsUISmokeTests
            WhatsUILayoutTests
            WhatsUIStateTests
            WhatsUIWindowTests
            WhatsUIScrollViewTests
            WhatsUILifecycleTests)
        target_link_libraries(${whatsui_runtime_test_target} PRIVATE WhatsCanvas::Software)
    endforeach()
endif()

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
