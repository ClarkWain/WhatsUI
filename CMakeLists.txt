cmake_minimum_required(VERSION 3.20)

project(WhatsUI LANGUAGES CXX)

option(WHATSUI_WITH_WHATSCANVAS "Link WhatsCanvas into the WhatsUI target" OFF)
option(WHATSUI_BUILD_TESTS "Build WhatsUI tests" ON)
option(WHATSUI_BUILD_EXAMPLES "Build WhatsUI examples (requires WHATSUI_WITH_WHATSCANVAS)" OFF)

add_library(WhatsUI
    src/whatsui/core/app.cpp
    src/whatsui/core/node.cpp
    src/whatsui/core/scheduler.cpp
    src/whatsui/core/text_metrics.cpp
    src/whatsui/core/theme.cpp
    src/whatsui/core/thread_check.cpp
    src/whatsui/input/events.cpp
    src/whatsui/runtime/runtime.cpp
    src/whatsui/widgets/button.cpp
    src/whatsui/widgets/image.cpp
    src/whatsui/widgets/layout.cpp
    src/whatsui/widgets/structural.cpp
    src/whatsui/widgets/text.cpp
    src/whatsui/widgets/text_input.cpp
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
    target_include_directories(WhatsUI PUBLIC "${WHATSUI_WHATSCANVAS_ROOT}/include")
endif()

# --- GLFW Platform Backend (requires WhatsCanvas + OpenGL) ---
if(WHATSUI_WITH_WHATSCANVAS)
    # Include the GLFW helper from WhatsCanvas
    include("${WHATSUI_WHATSCANVAS_ROOT}/cmake/WhatsCanvasOpenGL.cmake")
    whatscanvas_add_glfw_dependency("${WHATSUI_WHATSCANVAS_ROOT}")

    add_library(WhatsUIGlfw
        src/whatsui/platform/glfw_platform.cpp
    )
    add_library(WhatsUI::Glfw ALIAS WhatsUIGlfw)

    target_compile_features(WhatsUIGlfw PUBLIC cxx_std_17)

    target_link_libraries(WhatsUIGlfw
        PUBLIC
            WhatsUI::WhatsUI
            WhatsCanvas::OpenGL
            glfw
    )

    target_include_directories(WhatsUIGlfw
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()

if(WHATSUI_WITH_WHATSCANVAS AND WHATSUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(WHATSUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

