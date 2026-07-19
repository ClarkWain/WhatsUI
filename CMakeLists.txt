cmake_minimum_required(VERSION 3.20)

project(WhatsUI VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# WhatsUI and its vendored headers are UTF-8.  Pin MSVC's source and execution
# charset for every in-tree target: suppressing C4819 leaves the active ANSI
# code page in control and can corrupt preprocessing on non-English Windows.
if(MSVC)
    add_compile_options(/utf-8)
endif()

option(WHATSUI_WITH_WHATSCANVAS "Link WhatsCanvas into the WhatsUI target" OFF)
option(WHATSUI_BUILD_TESTS "Build WhatsUI tests" ON)
option(WHATSUI_BUILD_EXAMPLES "Build WhatsUI examples (requires WHATSUI_WITH_WHATSCANVAS)" OFF)
option(WHATSUI_INSTALL "Generate install and CMake package export rules" ON)
option(WHATSUI_ENABLE_SANITIZERS "Enable supported runtime memory sanitizers for WhatsUI and its dependents" OFF)
option(WHATSUI_ENABLE_ADVANCED_TEXT
    "Enable the in-tree WhatsCanvas FreeType/HarfBuzz text pipeline by default" ON)

set(WHATSUI_FLUENT_ICONS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/fluent-system-icons")
set(WHATSUI_FLUENT_ICONS_REGULAR
    "${WHATSUI_FLUENT_ICONS_DIR}/FluentSystemIcons-Regular.ttf")
set(WHATSUI_FLUENT_ICONS_FILLED
    "${WHATSUI_FLUENT_ICONS_DIR}/FluentSystemIcons-Filled.ttf")
foreach(whatsui_icon_font IN ITEMS
        "${WHATSUI_FLUENT_ICONS_REGULAR}"
        "${WHATSUI_FLUENT_ICONS_FILLED}")
    if(NOT EXISTS "${whatsui_icon_font}")
        message(FATAL_ERROR
            "WhatsUI's bundled Fluent icon font is missing: ${whatsui_icon_font}")
    endif()
endforeach()
file(SHA256 "${WHATSUI_FLUENT_ICONS_REGULAR}"
    WHATSUI_FLUENT_ICONS_REGULAR_SHA256)
file(SHA256 "${WHATSUI_FLUENT_ICONS_FILLED}"
    WHATSUI_FLUENT_ICONS_FILLED_SHA256)
if(NOT WHATSUI_FLUENT_ICONS_REGULAR_SHA256 STREQUAL
       "8ebc946d0de5ff7cd8fa653020f7c4046c71e27574ac6a9013aa727021290c32")
    message(FATAL_ERROR
        "FluentSystemIcons-Regular.ttf does not match the reviewed v1.1.328 asset.")
endif()
if(NOT WHATSUI_FLUENT_ICONS_FILLED_SHA256 STREQUAL
       "348ac72ae7a3ec4755f3aaba870bba15e64ed7a3adb522e4166f2a744451ac99")
    message(FATAL_ERROR
        "FluentSystemIcons-Filled.ttf does not match the reviewed v1.1.328 asset.")
endif()

set(WHATSUI_PACKAGE_REQUIRES_WHATSCANVAS OFF)
set(WHATSUI_PACKAGE_REQUIRES_GLFW OFF)

add_library(WhatsUI
    src/whatsui/core/accessibility.cpp
    src/whatsui/core/app.cpp
    src/whatsui/core/node.cpp
    src/whatsui/core/scheduler.cpp
    src/whatsui/core/text_metrics.cpp
    src/whatsui/core/theme.cpp
    src/whatsui/core/thread_check.cpp
    src/whatsui/core/ui_inspector.cpp
    src/whatsui/input/events.cpp
    src/whatsui/runtime/runtime.cpp
    src/whatsui/widgets/button.cpp
    src/whatsui/widgets/avatar.cpp
    src/whatsui/widgets/accordion.cpp
    src/whatsui/widgets/basic_controls.cpp
    src/whatsui/widgets/badge.cpp
    src/whatsui/widgets/card.cpp
    src/whatsui/widgets/date_time.cpp
    src/whatsui/widgets/dialog.cpp
    src/whatsui/widgets/drawer.cpp
    src/whatsui/widgets/feedback.cpp
    src/whatsui/widgets/form_feedback.cpp
    src/whatsui/widgets/icons.cpp
    src/whatsui/widgets/image.cpp
    src/whatsui/widgets/layout.cpp
    src/whatsui/widgets/label.cpp
    src/whatsui/widgets/list_view.cpp
    src/whatsui/widgets/navigation.cpp
    src/whatsui/widgets/overlays.cpp
    src/whatsui/widgets/popover.cpp
    src/whatsui/widgets/persona.cpp
    src/whatsui/widgets/rating.cpp
    src/whatsui/widgets/scroll_view.cpp
    src/whatsui/widgets/selection.cpp
    src/whatsui/widgets/structural.cpp
    src/whatsui/widgets/text.cpp
    src/whatsui/widgets/table.cpp
    src/whatsui/widgets/text_input.cpp
    src/whatsui/widgets/tree.cpp
    src/whatsui/widgets/virtual_list.cpp
)

add_library(WhatsUI::WhatsUI ALIAS WhatsUI)

target_compile_features(WhatsUI PUBLIC cxx_std_17)
target_compile_definitions(WhatsUI PRIVATE
    WHATSUI_FLUENT_ICONS_SOURCE_DIR="${WHATSUI_FLUENT_ICONS_DIR}"
)

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

    # Windows Fluent UI defaults to the platform's native text rasterizer.
    # Its ClearType hinting is materially sharper than FreeType's grayscale
    # atlas for small desktop UI text, and the basic backend caches native
    # bitmaps. Other platforms retain the portable FreeType default. An
    # explicit cache value still wins for embedders that need a different
    # text stack.
    if(WHATSUI_ENABLE_ADVANCED_TEXT)
        if(NOT DEFINED WHATSCANVAS_ENABLE_FREETYPE_RASTERIZER)
            if(WIN32)
                set(WHATSCANVAS_ENABLE_FREETYPE_RASTERIZER OFF CACHE BOOL
                    "Enable FreeType rasterization for WhatsUI text" )
            else()
                set(WHATSCANVAS_ENABLE_FREETYPE_RASTERIZER ON CACHE BOOL
                    "Enable FreeType rasterization for WhatsUI text" )
            endif()
        endif()
        if(NOT DEFINED WHATSCANVAS_ENABLE_OPENTYPE_SHAPING)
            set(WHATSCANVAS_ENABLE_OPENTYPE_SHAPING ON CACHE BOOL
                "Enable HarfBuzz shaping for WhatsUI text" )
        endif()
    endif()
    if(WHATSUI_INSTALL)
        # Let the dependency's install/export rules participate in the same
        # prefix so WhatsUIConfig can resolve WhatsCanvas::Software for an
        # external consumer. Ordinary source builds still keep the third-party
        # targets out of the default build graph.
        set(WHATSCANVAS_INSTALL_GLFW ON CACHE BOOL
            "Install GLFW when exporting WhatsUI::Glfw" FORCE)
        add_subdirectory(third_party/WhatsCanvas)
    else()
        add_subdirectory(third_party/WhatsCanvas EXCLUDE_FROM_ALL)
    endif()
    if(NOT TARGET WhatsCanvas::Software)
        message(FATAL_ERROR
            "WHATSUI_WITH_WHATSCANVAS=ON requires WhatsCanvas Software. "
            "Set WHATSCANVAS_BUILD_SOFTWARE=ON so headless tests and core paint symbols link correctly.")
    endif()
    # Widget implementations contain the Canvas-backed PaintContext path, so
    # static-library consumers must carry a concrete renderer transitively.
    # Without this, ordinary unit-test executables link WhatsUI successfully
    # until a widget object is pulled in, then fail with unresolved Canvas /
    # Paint / Image symbols.
    # A static library does not link its renderer until a final executable is
    # linked. Source-tree apps/tests link WhatsCanvas::Software explicitly;
    # the installed interface below supplies it to package consumers. Avoid a
    # build-tree target edge here because CMake requires every such edge to
    # belong to WhatsUI's export set.
    set_property(TARGET WhatsUI PROPERTY INTERFACE_LINK_LIBRARIES
        "$<INSTALL_INTERFACE:WhatsCanvas::Software>")
    target_compile_definitions(WhatsUI PUBLIC WHATSUI_HAS_WHATSCANVAS=1)
    target_include_directories(WhatsUI PUBLIC
        $<BUILD_INTERFACE:${WHATSUI_WHATSCANVAS_ROOT}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    set(WHATSUI_PACKAGE_REQUIRES_WHATSCANVAS ON)
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
            $<BUILD_INTERFACE:WhatsUI::WhatsUI>
            $<BUILD_INTERFACE:WhatsCanvas::OpenGL>
            $<BUILD_INTERFACE:glfw>
            $<INSTALL_INTERFACE:WhatsUI::WhatsUI>
            $<INSTALL_INTERFACE:WhatsCanvas::OpenGL>
            $<INSTALL_INTERFACE:glfw>
    )

    # The Windows GLFW host subclasses its native HWND to bridge IMM32
    # composition/candidate-window messages into TextInputSession. Keep this
    # private to the backend so headless and non-Windows consumers stay free
    # of Win32 dependencies.
    if(WIN32)
        target_sources(WhatsUIGlfw PRIVATE
            src/whatsui/platform/windows_uia_provider.cpp
        )
        target_link_libraries(WhatsUIGlfw PRIVATE imm32 oleaut32 uiautomationcore)
    endif()

    target_include_directories(WhatsUIGlfw
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    if(WHATSUI_INSTALL)
        set(WHATSUI_PACKAGE_REQUIRES_GLFW ON)
    endif()
endif()

if(WHATSUI_WITH_WHATSCANVAS AND WHATSUI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(WHATSUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Core packages may be installed headless or with the selected exported
# WhatsCanvas/GLFW dependency chain. The GLFW/OpenGL export has an external
# consumer smoke; native-window and GPU-driver validation remain release gates.
if(WHATSUI_INSTALL)
    set(WHATSUI_INSTALL_LICENSEDIR "${CMAKE_INSTALL_DATADIR}/licenses/WhatsUI")
    set(WHATSUI_INSTALL_TARGETS WhatsUI)
    if(TARGET WhatsUIGlfw)
        list(APPEND WHATSUI_INSTALL_TARGETS WhatsUIGlfw)
    endif()
    install(TARGETS ${WHATSUI_INSTALL_TARGETS}
        EXPORT WhatsUITargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/wui"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/NOTICE"
        "${CMAKE_CURRENT_SOURCE_DIR}/doc/whatsui/SBOM.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/doc/whatsui/FLUENT_ICONS.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/doc/whatsui/RELEASE_CHECKLIST.md"
        DESTINATION "${WHATSUI_INSTALL_LICENSEDIR}"
    )
    install(DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/fluent-system-icons/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/WhatsUI/fonts/fluent-system-icons"
        FILES_MATCHING
            PATTERN "*.ttf"
            PATTERN "*.json"
    )
    install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/fluent-system-icons/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/fluent-system-icons/NOTICE"
        DESTINATION "${WHATSUI_INSTALL_LICENSEDIR}/third_party/fluent-system-icons"
    )
    if(WHATSUI_WITH_WHATSCANVAS)
        install(FILES "${WHATSUI_WHATSCANVAS_ROOT}/LICENSE"
            DESTINATION "${WHATSUI_INSTALL_LICENSEDIR}/third_party/WhatsCanvas"
        )
    endif()

    install(EXPORT WhatsUITargets
        FILE WhatsUITargets.cmake
        NAMESPACE WhatsUI::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WhatsUI"
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WhatsUIConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/WhatsUIConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/WhatsUI"
        PATH_VARS WHATSUI_INSTALL_LICENSEDIR
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
