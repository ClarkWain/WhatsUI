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
