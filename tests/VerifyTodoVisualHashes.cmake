# Runs the deterministic WhatsCanvas Software Todo walkthrough and checks the
# four rendered scenes. Update these hashes only alongside an intentionally
# reviewed visual change; the test output prints every actual value for that
# workflow.

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

# Fluent Todo baseline, visually reviewed after the add/toggle/delete/clear
# walkthrough.  `todo_3` specifically exercises the structural rebuild that
# previously exposed the backend text-clip defect.
set(expected_todo_0 "d1b5184c458d9bc6c663d9a1dd97844a6e2ac89f2a479dcc416fb80eba616734")
set(expected_todo_1 "9aa5acb94fcef3b01c31e3b7107b4ab0f65342117b19150eaee77088dc7fc53b")
set(expected_todo_2 "b0adf94fe4e2c1c5b122a72b7de3cf9a064a86c8fe3fad487abe98d4762969ca")
set(expected_todo_3 "ca1d1113013b6149c466f93e2371858d77596084c8ad07a61dc4c46d613c24e3")

foreach(frame RANGE 0 3)
    set(image "${WHATSUI_TODO_OUTPUT_DIR}/todo_${frame}.ppm")
    if(NOT EXISTS "${image}")
        message(FATAL_ERROR "Todo visual walkthrough did not produce ${image}")
    endif()
    file(SHA256 "${image}" actual)
    if(NOT actual STREQUAL expected_todo_${frame})
        message(FATAL_ERROR
            "Todo visual regression in todo_${frame}.ppm: expected ${expected_todo_${frame}}, got ${actual}. "
            "Review the rendered artifact and update tests/VerifyTodoVisualHashes.cmake only for an intentional change.")
    endif()
endforeach()

message(STATUS "Todo Software visual baselines match.")
