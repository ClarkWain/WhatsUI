cmake_minimum_required(VERSION 3.20)

project(WhatsUIConsumerSmoke LANGUAGES CXX)

find_package(WhatsUI 0.1 CONFIG REQUIRED)

foreach(_whatsui_release_file IN ITEMS LICENSE NOTICE SBOM.md RELEASE_CHECKLIST.md)
    if(NOT EXISTS "${WhatsUI_LICENSE_DIR}/${_whatsui_release_file}")
        message(FATAL_ERROR
            "Installed WhatsUI package is missing release metadata: "
            "${WhatsUI_LICENSE_DIR}/${_whatsui_release_file}")
    endif()
endforeach()
if(WhatsUI_WITH_WHATSCANVAS AND
   NOT EXISTS "${WhatsUI_LICENSE_DIR}/third_party/WhatsCanvas/LICENSE")
    message(FATAL_ERROR
        "Installed WhatsUI package is missing the WhatsCanvas license: "
        "${WhatsUI_LICENSE_DIR}/third_party/WhatsCanvas/LICENSE")
endif()

add_executable(whatsui_consumer_smoke main.cpp)
set(WHATSUI_PACKAGE_TARGET "Core" CACHE STRING "Installed WhatsUI target to consume (Core or Glfw)")
set_property(CACHE WHATSUI_PACKAGE_TARGET PROPERTY STRINGS Core Glfw)
if(WHATSUI_PACKAGE_TARGET STREQUAL "Glfw")
    target_link_libraries(whatsui_consumer_smoke PRIVATE WhatsUI::Glfw)
    target_compile_definitions(whatsui_consumer_smoke PRIVATE WHATSUI_PACKAGE_USE_GLFW=1)
elseif(WHATSUI_PACKAGE_TARGET STREQUAL "Core")
    target_link_libraries(whatsui_consumer_smoke PRIVATE WhatsUI::WhatsUI)
else()
    message(FATAL_ERROR "WHATSUI_PACKAGE_TARGET must be Core or Glfw")
endif()
target_compile_features(whatsui_consumer_smoke PRIVATE cxx_std_17)
