# Produces reviewable Todo screenshots at the three Windows layout breakpoints.
#
# This is deliberately separate from VerifyTodoVisualHashes.cmake: hashes catch
# unintended pixel changes, while these captures make responsive layout changes
# visible to a reviewer.  The Todo capture executable owns the state script; it
# accepts an output path followed by `--size <logical-width>x<logical-height>`.

if(NOT DEFINED WHATSUI_TODO_EXECUTABLE OR NOT EXISTS "${WHATSUI_TODO_EXECUTABLE}")
    message(FATAL_ERROR "WHATSUI_TODO_EXECUTABLE must name a built WhatsUITodoApp executable")
endif()

if(NOT DEFINED WHATSUI_TODO_REVIEW_OUTPUT_DIR)
    message(FATAL_ERROR "WHATSUI_TODO_REVIEW_OUTPUT_DIR is required")
endif()

# Keep these representative of Windows app usage rather than merely sampling
# arbitrary pixel widths: compact portrait, common desktop, and a wide desktop.
set(review_names narrow regular wide)
set(review_sizes 360x720 640x560 1180x760)
set(review_pixel_sizes 720x1440 1280x1120 2360x1520)
# The final scene is the important structural-transition checkpoint: it is
# captured after a row deletion and a clear-completed operation have rebuilt
# both the ForEach and the empty-state If. It must reproduce the initial scene
# exactly at every breakpoint. This same-run invariant catches leaked backend
# clip/paint state without depending on the installed Windows font revision.
# The regular capture also owns a scroll-end artifact below: it proves the
# third task is reachable at the 640x560 Windows acceptance viewport.

file(REMOVE_RECURSE "${WHATSUI_TODO_REVIEW_OUTPUT_DIR}")

foreach(index RANGE 0 2)
    list(GET review_names ${index} name)
    list(GET review_sizes ${index} logical_size)
    list(GET review_pixel_sizes ${index} pixel_size)
    set(scene_dir "${WHATSUI_TODO_REVIEW_OUTPUT_DIR}/${name}")

    execute_process(
        COMMAND "${WHATSUI_TODO_EXECUTABLE}" "${scene_dir}" --size "${logical_size}"
        RESULT_VARIABLE render_result
        OUTPUT_VARIABLE render_output
        ERROR_VARIABLE render_error
    )
    if(NOT render_result EQUAL 0)
        message(FATAL_ERROR
            "Todo ${name} visual capture failed (${render_result}):\n${render_output}${render_error}")
    endif()

    foreach(frame RANGE 0 3)
        set(image "${scene_dir}/todo_${frame}.ppm")
        if(NOT EXISTS "${image}")
            message(FATAL_ERROR "Todo ${name} review capture did not produce ${image}")
        endif()

        # PPM's ASCII header makes the physical capture size testable without
        # relying on the same hashes as the pixel-regression suite.
        file(READ "${image}" ppm_header LIMIT 64)
        string(REGEX MATCH "^P6[\r\n ]+([0-9]+)[ ]+([0-9]+)[\r\n ]+255[\r\n]" ppm_match "${ppm_header}")
        if(NOT ppm_match)
            message(FATAL_ERROR "${image} is not a valid binary PPM capture")
        endif()
        set(actual_size "${CMAKE_MATCH_1}x${CMAKE_MATCH_2}")
        if(NOT actual_size STREQUAL pixel_size)
            message(FATAL_ERROR
                "${image} has physical size ${actual_size}; expected ${pixel_size} for ${logical_size} at 2x DPR")
        endif()

        file(SIZE "${image}" image_bytes)
        math(EXPR minimum_bytes "${CMAKE_MATCH_1} * ${CMAKE_MATCH_2} * 3")
        if(image_bytes LESS minimum_bytes)
            message(FATAL_ERROR "${image} is truncated (${image_bytes} bytes)")
        endif()

        file(SHA256 "${image}" actual_hash)
        set(actual_${name}_${frame} "${actual_hash}")
    endforeach()

    if(NOT actual_${name}_0 STREQUAL actual_${name}_3)
        message(FATAL_ERROR
            "Todo ${name} post-structural scene differs from its initial scene: "
            "todo_0=${actual_${name}_0}, todo_3=${actual_${name}_3}. "
            "Review the screenshots for leaked clip/paint/layout state.")
    endif()

    foreach(frame RANGE 0 2)
        math(EXPR next_frame "${frame} + 1")
        if(actual_${name}_${frame} STREQUAL actual_${name}_${next_frame})
            message(FATAL_ERROR
                "Todo ${name} walkthrough frames ${frame} and ${next_frame} are identical; "
                "the scripted UI transition was not rendered.")
        endif()
    endforeach()

    if(name STREQUAL "regular")
        set(scroll_end "${scene_dir}/todo_scroll_end.ppm")
        if(NOT EXISTS "${scroll_end}")
            message(FATAL_ERROR "Todo regular review did not produce the required scroll-end capture")
        endif()
        file(SHA256 "${scroll_end}" actual_scroll_end_hash)
        if(actual_scroll_end_hash STREQUAL actual_${name}_3)
            message(FATAL_ERROR
                "Todo regular scroll-end capture is identical to the final scene; "
                "the 640x560 reachable-task transition was not rendered.")
        endif()
    endif()
endforeach()

message(STATUS "Todo review captures written to ${WHATSUI_TODO_REVIEW_OUTPUT_DIR}")
