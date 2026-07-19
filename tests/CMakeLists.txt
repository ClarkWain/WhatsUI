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

add_executable(WhatsUITextAreaCompletionTests
    text_area_completion_tests.cpp
)
target_link_libraries(WhatsUITextAreaCompletionTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUITextAreaCompletionTests PRIVATE cxx_std_17)

if(WHATSUI_ENABLE_SANITIZERS)
    foreach(whatsui_test_target IN ITEMS
            WhatsUISmokeTests
            WhatsUILayoutTests
            WhatsUIStateTests
            WhatsUIWindowTests
            WhatsUIScrollViewTests
            WhatsUITextAreaCompletionTests)
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

add_test(
    NAME whatsui_text_area_completion_tests
    COMMAND WhatsUITextAreaCompletionTests
)

add_executable(WhatsUIFluentFieldMessageBarTests fluent_field_messagebar_tests.cpp)
target_link_libraries(WhatsUIFluentFieldMessageBarTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentFieldMessageBarTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_field_messagebar COMMAND WhatsUIFluentFieldMessageBarTests)
set_tests_properties(whatsui_fluent_field_messagebar PROPERTIES LABELS "fluent;field;messagebar;accessibility")

add_executable(WhatsUIRatingImageTests
    rating_image_tests.cpp
)
target_link_libraries(WhatsUIRatingImageTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIRatingImageTests PRIVATE cxx_std_17)
add_test(NAME whatsui_rating_image_tests COMMAND WhatsUIRatingImageTests)
set_tests_properties(whatsui_rating_image_tests PROPERTIES LABELS "fluent;rating;image;accessibility")

add_executable(WhatsUIFluentBadgeTests
    fluent_badge_tests.cpp
)
target_link_libraries(WhatsUIFluentBadgeTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentBadgeTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_badge_tests COMMAND WhatsUIFluentBadgeTests)
set_tests_properties(whatsui_fluent_badge_tests PROPERTIES LABELS "fluent;badge;accessibility")

add_executable(WhatsUIFluentRangeControlsTests
    fluent_range_controls_tests.cpp
)
target_link_libraries(WhatsUIFluentRangeControlsTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentRangeControlsTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_range_controls COMMAND WhatsUIFluentRangeControlsTests)
set_tests_properties(whatsui_fluent_range_controls PROPERTIES
    LABELS "fluent;divider;slider;progress;radio;switch;accessibility")

add_executable(WhatsUIFluentFeedbackTests
    fluent_feedback_tests.cpp
)
target_link_libraries(WhatsUIFluentFeedbackTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentFeedbackTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_feedback COMMAND WhatsUIFluentFeedbackTests)
set_tests_properties(whatsui_fluent_feedback PROPERTIES
    LABELS "fluent;toast;spinner;overlay;accessibility")

add_executable(WhatsUIFluentPopoverTests fluent_popover_tests.cpp)
target_link_libraries(WhatsUIFluentPopoverTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentPopoverTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_popover COMMAND WhatsUIFluentPopoverTests)

add_executable(WhatsUIFluentDrawerTests fluent_drawer_tests.cpp)
target_link_libraries(WhatsUIFluentDrawerTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentDrawerTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_drawer COMMAND WhatsUIFluentDrawerTests)
set_tests_properties(whatsui_fluent_drawer PROPERTIES LABELS "fluent;drawer;overlay;accessibility")

add_executable(WhatsUIFluentAccordionTests fluent_accordion_tests.cpp)
target_link_libraries(WhatsUIFluentAccordionTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentAccordionTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_accordion COMMAND WhatsUIFluentAccordionTests)
set_tests_properties(whatsui_fluent_accordion PROPERTIES LABELS "fluent;accordion;accessibility")

add_executable(WhatsUIFluentPersonaTests fluent_persona_tests.cpp)
target_link_libraries(WhatsUIFluentPersonaTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentPersonaTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_persona COMMAND WhatsUIFluentPersonaTests)
set_tests_properties(whatsui_fluent_persona PROPERTIES LABELS "fluent;persona;avatar;presence;accessibility")

add_executable(WhatsUIFluentDateTimeTests fluent_date_time_tests.cpp)
target_link_libraries(WhatsUIFluentDateTimeTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentDateTimeTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_date_time COMMAND WhatsUIFluentDateTimeTests)
set_tests_properties(whatsui_fluent_date_time PROPERTIES LABELS "fluent;calendar;datepicker;timepicker;accessibility")

add_executable(WhatsUIFluentTableTests fluent_table_tests.cpp)
target_link_libraries(WhatsUIFluentTableTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentTableTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_table COMMAND WhatsUIFluentTableTests)
set_tests_properties(whatsui_fluent_table PROPERTIES LABELS "fluent;table;datagrid;accessibility")

add_executable(WhatsUIFluentTreeTests fluent_tree_tests.cpp)
target_link_libraries(WhatsUIFluentTreeTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentTreeTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_tree COMMAND WhatsUIFluentTreeTests)
set_tests_properties(whatsui_fluent_tree PROPERTIES LABELS "fluent;tree;accessibility")

set_tests_properties(whatsui_fluent_popover PROPERTIES
    LABELS "fluent;popover;teachingpopover;overlay;accessibility")

add_executable(WhatsUIFluentNavigationTests fluent_navigation_tests.cpp)
target_link_libraries(WhatsUIFluentNavigationTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentNavigationTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_navigation COMMAND WhatsUIFluentNavigationTests)
set_tests_properties(whatsui_fluent_navigation PROPERTIES
    LABELS "fluent;toolbar;tabs;link;breadcrumb;accessibility")

add_executable(WhatsUIFluentSelectionControlsTests fluent_selection_controls_tests.cpp)
target_link_libraries(WhatsUIFluentSelectionControlsTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIFluentSelectionControlsTests PRIVATE cxx_std_17)
add_test(NAME whatsui_fluent_selection_controls_tests COMMAND WhatsUIFluentSelectionControlsTests)
set_tests_properties(whatsui_fluent_selection_controls_tests PROPERTIES
    LABELS "fluent;listbox;combobox;dropdown;accessibility")

add_executable(WhatsUIAvatarGroupTests avatar_group_tests.cpp)
target_link_libraries(WhatsUIAvatarGroupTests PRIVATE WhatsUI::WhatsUI)
target_compile_features(WhatsUIAvatarGroupTests PRIVATE cxx_std_17)
add_test(NAME whatsui_avatar_group_tests COMMAND WhatsUIAvatarGroupTests)
set_tests_properties(whatsui_avatar_group_tests PROPERTIES
    LABELS "fluent;avatar;group;presence;accessibility")

# WhatsUI is a static library. When the optional WhatsCanvas bridge is enabled,
# behavior tests that pull widget paint objects must also resolve the bridge's
# Canvas symbols even when they do not create a real Canvas themselves.
if(TARGET WhatsCanvasSoftware)
    target_link_libraries(WhatsUIFluentFieldMessageBarTests PRIVATE WhatsCanvas::Software)
    add_executable(WhatsUIFluentFieldMessageBarVisualTests fluent_field_messagebar_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentFieldMessageBarVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentFieldMessageBarVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_field_messagebar_visual
        COMMAND WhatsUIFluentFieldMessageBarVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_field_messagebar.ppm 1.0)
    add_test(NAME whatsui_fluent_field_messagebar_visual_150dpi
        COMMAND WhatsUIFluentFieldMessageBarVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_field_messagebar_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_field_messagebar_visual whatsui_fluent_field_messagebar_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;field;messagebar;dpi")
    target_link_libraries(WhatsUIFluentFeedbackTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentPopoverTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentDrawerTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentAccordionTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentPersonaTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentDateTimeTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentTableTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentTreeTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentNavigationTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentSelectionControlsTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIAvatarGroupTests PRIVATE WhatsCanvas::Software)

    add_executable(WhatsUIFluentDrawerVisualTests fluent_drawer_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentDrawerVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentDrawerVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_drawer_visual
        COMMAND WhatsUIFluentDrawerVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_drawer_review.ppm 1.0)
    add_test(NAME whatsui_fluent_drawer_visual_150dpi
        COMMAND WhatsUIFluentDrawerVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_drawer_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_drawer_visual whatsui_fluent_drawer_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;drawer;dpi")

    add_executable(WhatsUIFluentAccordionVisualTests fluent_accordion_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentAccordionVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentAccordionVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_accordion_visual
        COMMAND WhatsUIFluentAccordionVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_accordion_review.ppm 1.0)
    add_test(NAME whatsui_fluent_accordion_visual_150dpi
        COMMAND WhatsUIFluentAccordionVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_accordion_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_accordion_visual whatsui_fluent_accordion_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;accordion;dpi")

    add_executable(WhatsUIFluentPersonaVisualTests fluent_persona_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentPersonaVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentPersonaVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_persona_visual
        COMMAND WhatsUIFluentPersonaVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_persona_review.ppm 1.0)
    add_test(NAME whatsui_fluent_persona_visual_150dpi
        COMMAND WhatsUIFluentPersonaVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_persona_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_persona_visual whatsui_fluent_persona_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;persona;avatar;presence;dpi")

    add_executable(WhatsUIFluentDateTimeVisualTests fluent_date_time_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentDateTimeVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentDateTimeVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_date_time_visual
        COMMAND WhatsUIFluentDateTimeVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_date_time_review.ppm 1.0)
    add_test(NAME whatsui_fluent_date_time_visual_150dpi
        COMMAND WhatsUIFluentDateTimeVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_date_time_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_date_time_visual whatsui_fluent_date_time_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;calendar;datepicker;timepicker;dpi")

    add_executable(WhatsUIFluentTableVisualTests fluent_table_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentTableVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentTableVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_table_visual
        COMMAND WhatsUIFluentTableVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_table_review.ppm 1.0)
    add_test(NAME whatsui_fluent_table_visual_150dpi
        COMMAND WhatsUIFluentTableVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_table_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_table_visual whatsui_fluent_table_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;table;datagrid;dpi")

    add_executable(WhatsUIFluentTreeVisualTests fluent_tree_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentTreeVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentTreeVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_tree_visual
        COMMAND WhatsUIFluentTreeVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_tree_review.ppm 1.0)
    add_test(NAME whatsui_fluent_tree_visual_150dpi
        COMMAND WhatsUIFluentTreeVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_tree_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_tree_visual whatsui_fluent_tree_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;tree;dpi")

    add_executable(WhatsUIFluentFeedbackVisualTests
        fluent_feedback_visual_tests.cpp
    )
    target_link_libraries(WhatsUIFluentFeedbackVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentFeedbackVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_feedback_visual
        COMMAND WhatsUIFluentFeedbackVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_feedback_review.ppm 1.0)
    add_test(NAME whatsui_fluent_feedback_visual_150dpi
        COMMAND WhatsUIFluentFeedbackVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_feedback_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_feedback_visual whatsui_fluent_feedback_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;toast;spinner;dpi")

    add_executable(WhatsUIFluentPopoverVisualTests fluent_popover_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentPopoverVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentPopoverVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_popover_visual
        COMMAND WhatsUIFluentPopoverVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_popover_review)
    set_tests_properties(whatsui_fluent_popover_visual
        PROPERTIES LABELS "visual;software;fluent;popover;teachingpopover;dpi")

    add_executable(WhatsUIFluentNavigationVisualTests fluent_navigation_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentNavigationVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentNavigationVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_navigation_visual
        COMMAND WhatsUIFluentNavigationVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_navigation_review)
    add_test(NAME whatsui_fluent_navigation_visual_150dpi
        COMMAND WhatsUIFluentNavigationVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_navigation_review_150dpi 1.5)
    set_tests_properties(whatsui_fluent_navigation_visual whatsui_fluent_navigation_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;toolbar;tabs;link;breadcrumb;dpi")

    add_executable(WhatsUIFluentSelectionControlsVisualTests fluent_selection_controls_visual_tests.cpp)
    target_link_libraries(WhatsUIFluentSelectionControlsVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentSelectionControlsVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_selection_controls_visual
        COMMAND WhatsUIFluentSelectionControlsVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_selection_controls.ppm 1.0)
    add_test(NAME whatsui_fluent_selection_controls_visual_150dpi
        COMMAND WhatsUIFluentSelectionControlsVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_selection_controls_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_selection_controls_visual whatsui_fluent_selection_controls_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;listbox;combobox;dropdown;dpi")

    add_executable(WhatsUIAvatarGroupVisualTests avatar_group_visual_tests.cpp)
    target_link_libraries(WhatsUIAvatarGroupVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIAvatarGroupVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_avatar_group_visual
        COMMAND WhatsUIAvatarGroupVisualTests ${CMAKE_CURRENT_BINARY_DIR}/avatar_group_review.ppm 1.0)
    add_test(NAME whatsui_avatar_group_visual_150dpi
        COMMAND WhatsUIAvatarGroupVisualTests ${CMAKE_CURRENT_BINARY_DIR}/avatar_group_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_avatar_group_visual whatsui_avatar_group_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;avatar;group;presence;dpi")

    target_link_libraries(WhatsUIRatingImageTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentBadgeTests PRIVATE WhatsCanvas::Software)
    target_link_libraries(WhatsUIFluentRangeControlsTests PRIVATE WhatsCanvas::Software)
endif()

# Exercise the public WhatsCanvas bridge rather than WhatsCanvas internals.
# This is deliberately Windows-font-aware: it proves the default fallback
# chain and Segoe UI Emoji policy when that system font is installed, while
# still allowing a reduced CI image to validate UTF-8/bidi/line layout.
if(TARGET WhatsCanvasSoftware)
    add_executable(WhatsUIFluentCheckboxCompletionTests
        fluent_checkbox_completion_tests.cpp
    )
    target_link_libraries(WhatsUIFluentCheckboxCompletionTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentCheckboxCompletionTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_checkbox_completion
        COMMAND WhatsUIFluentCheckboxCompletionTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_checkbox_review.ppm 1.0)
    add_test(NAME whatsui_fluent_checkbox_completion_150dpi
        COMMAND WhatsUIFluentCheckboxCompletionTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_checkbox_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_checkbox_completion whatsui_fluent_checkbox_completion_150dpi
        PROPERTIES LABELS "visual;software;fluent;checkbox;accessibility")

    add_executable(WhatsUIFluentRangeControlsVisualTests
        fluent_range_controls_visual_tests.cpp
    )
    target_link_libraries(WhatsUIFluentRangeControlsVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentRangeControlsVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_range_controls_visual
        COMMAND WhatsUIFluentRangeControlsVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_range_controls_review.ppm 1.0)
    add_test(NAME whatsui_fluent_range_controls_visual_150dpi
        COMMAND WhatsUIFluentRangeControlsVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_range_controls_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_range_controls_visual whatsui_fluent_range_controls_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;divider;slider;progress;radio;switch;dpi")

    add_executable(WhatsUIFluentRatingImageVisualTests
        fluent_rating_image_visual_tests.cpp
    )
    target_link_libraries(WhatsUIFluentRatingImageVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentRatingImageVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_rating_image_visual
        COMMAND WhatsUIFluentRatingImageVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_rating_image_review.ppm 1.0)
    add_test(NAME whatsui_fluent_rating_image_visual_150dpi
        COMMAND WhatsUIFluentRatingImageVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_rating_image_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_rating_image_visual whatsui_fluent_rating_image_visual_150dpi PROPERTIES
        LABELS "visual;software;fluent;rating;image")

    add_executable(WhatsUIFluentBadgeVisualTests
        fluent_badge_visual_tests.cpp
    )
    target_link_libraries(WhatsUIFluentBadgeVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentBadgeVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_badge_visual
        COMMAND WhatsUIFluentBadgeVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_badge_review.ppm 1.0)
    add_test(NAME whatsui_fluent_badge_visual_150dpi
        COMMAND WhatsUIFluentBadgeVisualTests ${CMAKE_CURRENT_BINARY_DIR}/fluent_badge_review_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_badge_visual whatsui_fluent_badge_visual_150dpi
        PROPERTIES LABELS "visual;software;fluent;badge;dpi")

    add_executable(WhatsUIFluentMenuButtonVisualTests
        fluent_menu_button_visual_tests.cpp
    )
    target_link_libraries(WhatsUIFluentMenuButtonVisualTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentMenuButtonVisualTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_menu_button_visual
        COMMAND WhatsUIFluentMenuButtonVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_menu_button_review.ppm)
    set_tests_properties(whatsui_fluent_menu_button_visual PROPERTIES
        LABELS "visual;software;fluent;menu")

    add_executable(WhatsUIFluentCardTests
        fluent_card_tests.cpp
    )
    target_link_libraries(WhatsUIFluentCardTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentCardTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_card_tests
        COMMAND WhatsUIFluentCardTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_card_review.ppm)
    set_tests_properties(whatsui_fluent_card_tests PROPERTIES LABELS "card;visual;software;fluent")

    add_executable(WhatsUIFluentComponentVisualMatrixTests
        fluent_component_visual_matrix_tests.cpp
    )
    target_link_libraries(WhatsUIFluentComponentVisualMatrixTests PRIVATE WhatsUI::WhatsUI WhatsCanvas::Software)
    target_compile_features(WhatsUIFluentComponentVisualMatrixTests PRIVATE cxx_std_17)
    add_test(NAME whatsui_fluent_component_visual_matrix
        COMMAND WhatsUIFluentComponentVisualMatrixTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_component_visual_matrix.ppm 1.0)
    add_test(NAME whatsui_fluent_component_visual_matrix_150dpi
        COMMAND WhatsUIFluentComponentVisualMatrixTests
            ${CMAKE_CURRENT_BINARY_DIR}/fluent_component_visual_matrix_150dpi.ppm 1.5)
    set_tests_properties(whatsui_fluent_component_visual_matrix PROPERTIES LABELS "visual;software;fluent")
    set_tests_properties(whatsui_fluent_component_visual_matrix_150dpi PROPERTIES LABELS "visual;software;fluent;dpi")

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

    add_executable(WhatsUITextAreaVisualTests
        text_area_visual_tests.cpp
    )
    target_link_libraries(WhatsUITextAreaVisualTests
        PRIVATE
            WhatsUI::WhatsUI
            WhatsCanvas::Software
    )
    target_compile_features(WhatsUITextAreaVisualTests PRIVATE cxx_std_17)
    add_test(
        NAME whatsui_text_area_visual_tests
        COMMAND WhatsUITextAreaVisualTests
            ${CMAKE_CURRENT_BINARY_DIR}/textarea_fluent_states.ppm
    )
    set_tests_properties(whatsui_text_area_visual_tests PROPERTIES
        LABELS "text;ime;visual;software;fluent"
    )
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

add_executable(WhatsUIMenuButtonCompletionTests
    menu_button_completion_tests.cpp
)

target_link_libraries(WhatsUIMenuButtonCompletionTests
    PRIVATE
        WhatsUI::WhatsUI
)

target_compile_features(WhatsUIMenuButtonCompletionTests PRIVATE cxx_std_17)

add_test(
    NAME whatsui_menu_button_completion_tests
    COMMAND WhatsUIMenuButtonCompletionTests
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
            WhatsUITextAreaCompletionTests
            WhatsUIRatingImageTests
            WhatsUILifecycleTests
            WhatsUIThemeTests
            WhatsUIFrameStatsTests
            WhatsUIFluentCardTests
            WhatsUIBasicControlsTests
            WhatsUIOverlayTests
            WhatsUIMenuButtonCompletionTests
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
        # Software readback creates three complete multi-scene 2x walkthroughs.
        # The review job is intentionally comprehensive and can exceed CTest's
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
