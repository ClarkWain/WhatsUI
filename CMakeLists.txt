cmake_minimum_required(VERSION 3.20)

project(WhatsUI VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(WHATSUI_WITH_WHATSCANVAS "Link WhatsCanvas into the WhatsUI target" OFF)
option(WHATSUI_BUILD_TESTS "Build WhatsUI tests" ON)
option(WHATSUI_BUILD_EXAMPLES "Build WhatsUI examples (requires WHATSUI_WITH_WHATSCANVAS)" OFF)
option(WHATSUI_INSTALL "Generate install and CMake package export rules" ON)
option(WHATSUI_ENABLE_SANITIZERS "Enable supported runtime memory sanitizers for WhatsUI and its dependents" OFF)

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
    src/whatsui/widgets/dialog.cpp
    src/whatsui/widgets/image.cpp
    src/whatsui/widgets/layout.cpp
    src/whatsui/widgets/scroll_view.cpp
    src/whatsui/widgets/structural.cpp
    src/whatsui/widgets/text.cpp
    src/whatsui/widgets/text_input.cpp
)

add_library(WhatsUI::WhatsUI ALIAS WhatsUI)

target_compile_features(WhatsUI PUBLIC cxx_std_17)

if(WHATSUI_ENABLE_SANITIZERS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/WhatsUISanitizers.cmake")
    whatsui_enable_sanitizers(WhatsUI)
endif()

target_include_directories(WhatsUI
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(WhatsUI PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
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

# The developer-preview package is deliberately headless.  The optional
# WhatsCanvas integration currently consumes its in-tree subproject directly;
# publishing that integration requires a separately packaged WhatsCanvas
# dependency and is therefore kept out of this first consumer contract.
if(WHATSUI_INSTALL)
    if(WHATSUI_WITH_WHATSCANVAS)
        message(WARNING
            "WhatsUI install/export is unavailable with WHATSUI_WITH_WHATSCANVAS=ON. "
            "Build the developer-preview package with WHATSUI_WITH_WHATSCANVAS=OFF.")
    else()
        install(TARGETS WhatsUI
            EXPORT WhatsUITargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/wui"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        )

        install(EXPORT WhatsUITargets
            FILE WhatsUITargets.cmake
            NAMESPACE WhatsUI::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WhatsUI"
        )

        configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WhatsUIConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/WhatsUIConfig.cmake"
            INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WhatsUI"
        )
        write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/WhatsUIConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
        )
        install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/WhatsUIConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/WhatsUIConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WhatsUI"
        )
    endif()
endif()
