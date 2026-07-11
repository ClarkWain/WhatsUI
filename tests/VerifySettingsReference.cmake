# Validates the documented Software capture entry point for the M1 Settings
# reference. Pixels are reviewed deliberately rather than treated as a frozen
# baseline while M1's visual language is still evolving.

if(NOT DEFINED WHATSUI_SETTINGS_EXECUTABLE OR NOT EXISTS "${WHATSUI_SETTINGS_EXECUTABLE}")
    message(FATAL_ERROR "WHATSUI_SETTINGS_EXECUTABLE must name a built WhatsUISettingsApp executable")
endif()

file(REMOVE_RECURSE "${WHATSUI_SETTINGS_OUTPUT_DIR}")
execute_process(
    COMMAND "${WHATSUI_SETTINGS_EXECUTABLE}" "${WHATSUI_SETTINGS_OUTPUT_DIR}" --size 900x680
    RESULT_VARIABLE render_result
    OUTPUT_VARIABLE render_output
    ERROR_VARIABLE render_error
)
if(NOT render_result EQUAL 0)
    message(FATAL_ERROR "Settings reference capture failed (${render_result}):\n${render_output}${render_error}")
endif()

set(image "${WHATSUI_SETTINGS_OUTPUT_DIR}/settings_general.ppm")
if(NOT EXISTS "${image}")
    message(FATAL_ERROR "Settings reference did not produce settings_general.ppm")
endif()

file(READ "${image}" ppm_header LIMIT 64)
string(REGEX MATCH "^P6[\r\n ]+([0-9]+)[ ]+([0-9]+)[\r\n ]+255[\r\n]" ppm_match "${ppm_header}")
if(NOT ppm_match)
    message(FATAL_ERROR "${image} is not a valid binary PPM capture")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL "1800" OR NOT CMAKE_MATCH_2 STREQUAL "1360")
    message(FATAL_ERROR "${image} has physical size ${CMAKE_MATCH_1}x${CMAKE_MATCH_2}; expected 1800x1360 at 2x DPR")
endif()

file(SIZE "${image}" image_bytes)
if(image_bytes LESS 7344017)
    message(FATAL_ERROR "${image} is truncated (${image_bytes} bytes)")
endif()

message(STATUS "Settings reference Software capture is valid.")
