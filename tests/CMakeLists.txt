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

# Native OpenGL regression for the GLFW framebuffer-size callback.  It
# validates that RenderSurface::resize updates GL_VIEWPORT after the default
# framebuffer changes, preventing the resized Todo window from rendering only
# into its former lower-left area.  The test is intentionally not a headless
# substitute: unavailable desktop OpenGL is reported by CTest as skipped.
if(TARGET WhatsUI::Glfw)
    add_executable(WhatsUIGlfwResizeTests
        glfw_resize_tests.cpp
    )

    target_link_libraries(WhatsUIGlfwResizeTests
        PRIVATE
            WhatsUI::Glfw
    )

    target_compile_features(WhatsUIGlfwResizeTests PRIVATE cxx_std_17)

    add_test(
        NAME whatsui_glfw_resize_tests
        COMMAND WhatsUIGlfwResizeTests
    )
    set_tests_properties(whatsui_glfw_resize_tests PROPERTIES
        LABELS "native;glfw;opengl;resize"
        SKIP_RETURN_CODE 77
        TIMEOUT 30
    )
endif()

# Exercise the real Win32 UI Automation client boundary against a GLFW HWND.
# It gates the HWND root and the native projection of a named, focused child;
# the UIA client runs away from the GLFW window thread while that thread pumps.
if(WIN32 AND TARGET WhatsUI::Glfw)
    add_executable(WhatsUIWindowsUiaNativeSmokeTests
        windows_uia_native_smoke_tests.cpp
    )

    target_link_libraries(WhatsUIWindowsUiaNativeSmokeTests
        PRIVATE
            WhatsUI::Glfw
            uiautomationcore
            ole32
            oleaut32
            user32
    )

    target_compile_features(WhatsUIWindowsUiaNativeSmokeTests PRIVATE cxx_std_17)

    add_test(
        NAME whatsui_windows_uia_native_smoke_tests
        COMMAND WhatsUIWindowsUiaNativeSmokeTests
    )
    set_tests_properties(whatsui_windows_uia_native_smoke_tests PROPERTIES
        LABELS "native;windows;uia;accessibility;glfw"
        SKIP_RETURN_CODE 77
        TIMEOUT 30
    )
endif()

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

add_executable(WhatsUIThemeTests
    m3_theme_tests.cpp
)

target_link_libraries(WhatsUIThemeTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIThemeTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_theme_tests
    COMMAND WhatsUIThemeTests
)

add_executable(WhatsUIFrameStatsTests
    m4_frame_stats_tests.cpp
)

target_link_libraries(WhatsUIFrameStatsTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIFrameStatsTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_frame_stats_tests
    COMMAND WhatsUIFrameStatsTests
)

add_executable(WhatsUIBasicControlsTests
    m3_basic_controls_tests.cpp
)

target_link_libraries(WhatsUIBasicControlsTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIBasicControlsTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_basic_controls_tests
    COMMAND WhatsUIBasicControlsTests
)

add_executable(WhatsUIOverlayTests
    m3_overlay_tests.cpp
)

target_link_libraries(WhatsUIOverlayTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIOverlayTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_overlay_tests
    COMMAND WhatsUIOverlayTests
)

add_executable(WhatsUIAccessibilityTests
    m3_accessibility_tests.cpp
)

target_link_libraries(WhatsUIAccessibilityTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIAccessibilityTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_accessibility_tests
    COMMAND WhatsUIAccessibilityTests
)

add_executable(WhatsUIListViewTests
    m3_list_view_tests.cpp
)

target_link_libraries(WhatsUIListViewTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIListViewTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_list_view_tests
    COMMAND WhatsUIListViewTests
)

add_executable(WhatsUITodoStorageTests
    todo_storage_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app/todo_storage.cpp
)

target_include_directories(WhatsUITodoStorageTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app
)

target_compile_features(WhatsUITodoStorageTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_todo_storage_tests
    COMMAND WhatsUITodoStorageTests
)

add_executable(WhatsUITodoControllerTests
    todo_controller_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app/todo_controller.cpp
)

target_include_directories(WhatsUITodoControllerTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app
)

target_compile_features(WhatsUITodoControllerTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_todo_controller_tests
    COMMAND WhatsUITodoControllerTests
)

add_executable(WhatsUITodoInteractionTests
    todo_interaction_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app/todo_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app/todo_interaction.cpp
)

target_include_directories(WhatsUITodoInteractionTests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app
)

target_compile_features(WhatsUITodoInteractionTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_todo_interaction_tests
    COMMAND WhatsUITodoInteractionTests
)

# This drives the actual Todo declarative tree through UiWindow input routing.
# The test-only definition suppresses the sample executable entry point while
# retaining the exact buildTodoUi() implementation used by both Todo binaries.
set(WHATSUI_TODO_CANVAS_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/WhatsCanvas/include")
if(EXISTS "${WHATSUI_TODO_CANVAS_INCLUDE_DIR}/wsc/Canvas.h")
    add_executable(WhatsUITodoUiInteractionSmokeTests
        todo_ui_interaction_smoke_tests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app/todo_controller.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app/todo_interaction.cpp
    )

    target_include_directories(WhatsUITodoUiInteractionSmokeTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../examples/todo_app
            # main.cpp owns the shared Todo tree and includes Canvas.h even
            # when this test selects its interactive (non-rendering) path.
            ${WHATSUI_TODO_CANVAS_INCLUDE_DIR}
    )

    target_link_libraries(WhatsUITodoUiInteractionSmokeTests
        PRIVATE
            WhatsUI::WhatsUI
    )

    if(TARGET WhatsCanvas::Software)
        target_link_libraries(WhatsUITodoUiInteractionSmokeTests PRIVATE WhatsCanvas::Software)
    endif()

    target_compile_features(WhatsUITodoUiInteractionSmokeTests PRIVATE cxx_std_17)

    add_test(
        NAME whatsui_todo_ui_interaction_smoke_tests
        COMMAND WhatsUITodoUiInteractionSmokeTests
    )
    # This is a headless input-routing test and normally completes in a few
    # milliseconds.  Bound it so a Debug CRT dialog or unexpected modal
    # platform regression cannot stall an unattended full CTest run.
    set_tests_properties(whatsui_todo_ui_interaction_smoke_tests PROPERTIES
        TIMEOUT 30
    )
endif()

add_executable(WhatsUIVirtualListTests
    m4_virtual_list_tests.cpp
)

target_link_libraries(WhatsUIVirtualListTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIVirtualListTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_virtual_list_tests
    COMMAND WhatsUIVirtualListTests
)

add_executable(WhatsUIUiInspectorTests
    m4_ui_inspector_tests.cpp
)

target_link_libraries(WhatsUIUiInspectorTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIUiInspectorTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_ui_inspector_tests
    COMMAND WhatsUIUiInspectorTests
)

add_executable(WhatsUIBenchmarks
    m4_benchmarks.cpp
)

target_link_libraries(WhatsUIBenchmarks
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIBenchmarks PRIVATE cxx_std_17)

add_test(
    NAME whatsui_benchmarks
    COMMAND WhatsUIBenchmarks
)
set_tests_properties(whatsui_benchmarks PROPERTIES LABELS "benchmark;diagnostic")

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
            WhatsUILifecycleTests
            WhatsUIThemeTests
            WhatsUIFrameStatsTests
            WhatsUIBasicControlsTests
            WhatsUIOverlayTests
            WhatsUIAccessibilityTests
            WhatsUIListViewTests
            WhatsUITodoStorageTests
            WhatsUITodoControllerTests
            WhatsUITodoInteractionTests
            WhatsUIVirtualListTests
            WhatsUIUiInspectorTests
            WhatsUIBenchmarks)
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

# Native Todo performance smoke.  Register it only when the optional GLFW
# example exists; headless/source-only builds must not advertise a test they
# cannot build.  The CMake runner asserts both lifecycle markers, retains all
# child output for a diagnosable failure and makes a missing graphics desktop
# an explicit CTest skip instead of a pass.
if(TARGET WhatsUITodoGlfw)
    add_test(
        NAME whatsui_todo_native_perf_smoke
        COMMAND ${CMAKE_COMMAND}
            -DWHATSUI_TODO_GLFW_EXECUTABLE=$<TARGET_FILE:WhatsUITodoGlfw>
            -DWHATSUI_TODO_NATIVE_PERF_WORKING_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/todo_native_perf_smoke
            -P ${CMAKE_CURRENT_SOURCE_DIR}/RunTodoNativePerfSmoke.cmake
    )
    set_tests_properties(whatsui_todo_native_perf_smoke PROPERTIES
        LABELS "native;glfw;opengl;performance;todo"
        SKIP_REGULAR_EXPRESSION "SKIP: native GLFW/OpenGL surface unavailable"
        # The child runner has a 15-second watchdog; leave small process
        # teardown/reporting headroom while still catching a stuck runner.
        TIMEOUT 18
    )
endif()

if(TARGET WhatsUIDebugInspectorApp)
    add_test(
        NAME whatsui_inspector_reference_capture
        COMMAND $<TARGET_FILE:WhatsUIDebugInspectorApp>
            ${CMAKE_CURRENT_BINARY_DIR}/debug_inspector_reference_capture
    )
    set_tests_properties(whatsui_inspector_reference_capture PROPERTIES
        LABELS "visual;software;inspector"
    )
endif()
