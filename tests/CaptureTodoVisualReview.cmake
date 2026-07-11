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
set(review_sizes 360x720 640x720 1180x760)
set(review_pixel_sizes 720x1440 1280x1440 2360x1520)
# The final scene is the important structural-transition checkpoint: it is
# captured after a row deletion and a clear-completed operation have rebuilt
# both the ForEach and the empty-state If.  Keep a pixel baseline for that
# scene at every responsive breakpoint.  A valid-size PPM alone cannot catch
# a leaked backend clip that reduces labels to glyph fragments.
set(review_final_hashes
    "dad6a0c73abcb4fd8e159f43b263ee0a3ab6327dddedc44b8f5ec556325f746c"
    "9ed83d863b98ae2c0da8c759d525822a6a59ab82976315a3e1c3bf9f87061417"
    "52b6e8c8a3d795b601db6f5cfad480e8d7331c0e2f0919ba634b8d7bcc07f228")

file(REMOVE_RECURSE "${WHATSUI_TODO_REVIEW_OUTPUT_DIR}")

foreach(index RANGE 0 2)
    list(GET review_names ${index} name)
    list(GET review_sizes ${index} logical_size)
    list(GET review_pixel_sizes ${index} pixel_size)
    list(GET review_final_hashes ${index} expected_final_hash)
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

        # `todo_3` is the post-delete/post-clear checkpoint described above.
        # Pin it independently at each size so structural paint-state leaks
        # cannot hide behind a successful default-size visual hash.
        if(frame EQUAL 3)
            file(SHA256 "${image}" actual_final_hash)
            if(NOT actual_final_hash STREQUAL expected_final_hash)
                message(FATAL_ERROR
                    "Todo ${name} post-structural visual regression in todo_3.ppm: "
                    "expected ${expected_final_hash}, got ${actual_final_hash}. "
                    "Review the screenshot before accepting a new baseline.")
            endif()
        endif()
    endforeach()
endforeach()

message(STATUS "Todo review captures written to ${WHATSUI_TODO_REVIEW_OUTPUT_DIR}")
