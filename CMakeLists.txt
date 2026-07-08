cmake_minimum_required(VERSION 3.20)

project(WhatsUI LANGUAGES CXX)

option(WHATSUI_WITH_WHATSCANVAS "Link WhatsCanvas into the WhatsUI target" OFF)
option(WHATSUI_BUILD_TESTS "Build WhatsUI tests" ON)

add_library(WhatsUI
    src/whatsui/core/app.cpp
    src/whatsui/core/node.cpp
    src/whatsui/input/events.cpp
    src/whatsui/runtime/runtime.cpp
    src/whatsui/widgets/text_input.cpp
    src/whatsui/widgets/widgets.cpp
)

add_library(WhatsUI::WhatsUI ALIAS WhatsUI)

target_compile_features(WhatsUI PUBLIC cxx_std_17)

target_include_directories(WhatsUI
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WHATSUI_WITH_WHATSCANVAS)
    set(WHATSUI_WHATSCANVAS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/WhatsCanvas")
    if(NOT EXISTS "${WHATSUI_WHATSCANVAS_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "WHATSUI_WITH_WHATSCANVAS=ON requires third_party/WhatsCanvas to exist in the workspace.")
    endif()

    if(NOT EXISTS "${WHATSUI_WHATSCANVAS_ROOT}/third_party/glad/src/glad.c" OR
       NOT EXISTS "${WHATSUI_WHATSCANVAS_ROOT}/third_party/glad/include/glad/glad.h" OR
       NOT EXISTS "${WHATSUI_WHATSCANVAS_ROOT}/third_party/glm/glm/glm.hpp" OR
       NOT EXISTS "${WHATSUI_WHATSCANVAS_ROOT}/third_party/stb/stb_image.h")
        message(FATAL_ERROR
            "WHATSUI_WITH_WHATSCANVAS=ON requires initialized WhatsCanvas submodules. "
            "Run: git submodule update --init --recursive"
        )
    endif()

    add_subdirectory(third_party/WhatsCanvas EXCLUDE_FROM_ALL)
    target_compile_definitions(WhatsUI PUBLIC WHATSUI_HAS_WHATSCANVAS=1)
    target_link_libraries(WhatsUI PUBLIC WhatsCanvas::Software)
endif()

if(WHATSUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

