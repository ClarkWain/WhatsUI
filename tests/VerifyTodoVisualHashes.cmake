# Runs the deterministic WhatsCanvas Software Todo walkthrough and checks the
# four rendered scenes. The Windows build intentionally uses the platform's
# native Segoe UI rasterizer, so whole-image golden hashes are not portable
# across Windows/font revisions. Instead, assert the stronger state invariant:
# after add/toggle/delete/clear, the rebuilt final scene must be pixel-identical
# to the initial scene on the same machine. Intermediate scenes must remain
# distinct, and CI uploads the responsive captures for human visual review.

if(NOT DEFINED WHATSUI_TODO_EXECUTABLE OR NOT EXISTS "${WHATSUI_TODO_EXECUTABLE}")
    message(FATAL_ERROR "WHATSUI_TODO_EXECUTABLE must name a built WhatsUITodoApp executable")
endif()

file(REMOVE_RECURSE "${WHATSUI_TODO_OUTPUT_DIR}")
execute_process(
    COMMAND "${WHATSUI_TODO_EXECUTABLE}" "${WHATSUI_TODO_OUTPUT_DIR}"
    RESULT_VARIABLE render_result
    OUTPUT_VARIABLE render_output
    ERROR_VARIABLE render_error
)
if(NOT render_result EQUAL 0)
    message(FATAL_ERROR "Todo visual walkthrough failed (${render_result}):\n${render_output}${render_error}")
endif()

foreach(frame RANGE 0 3)
    set(image "${WHATSUI_TODO_OUTPUT_DIR}/todo_${frame}.ppm")
    if(NOT EXISTS "${image}")
        message(FATAL_ERROR "Todo visual walkthrough did not produce ${image}")
    endif()
    file(SHA256 "${image}" actual)
    set(actual_todo_${frame} "${actual}")
endforeach()

if(NOT actual_todo_0 STREQUAL actual_todo_3)
    message(FATAL_ERROR
        "Todo post-structural scene does not match its initial state: "
        "todo_0=${actual_todo_0}, todo_3=${actual_todo_3}. "
        "Review the rendered artifacts for leaked clip/paint/layout state.")
endif()

foreach(frame RANGE 0 2)
    math(EXPR next_frame "${frame} + 1")
    if(actual_todo_${frame} STREQUAL actual_todo_${next_frame})
        message(FATAL_ERROR
            "Todo walkthrough frames ${frame} and ${next_frame} are identical; "
            "the scripted UI transition was not rendered.")
    endif()
endforeach()

message(STATUS "Todo Software visual state invariants match.")
