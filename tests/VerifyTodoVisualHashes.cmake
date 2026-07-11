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
set(expected_todo_0 "92b099f26fab59df64720a6f77f9b1cc4fc7507ab843753314290db13bd935a2")
set(expected_todo_1 "fb89a764fa65cc1829ec1cf9a87dadb7b5d436d6179370a59c5a490dcb8dc4c3")
set(expected_todo_2 "5ae124f61d8e4284bc9b1cf6d63471fdccadb4721f3b33cf72d57c9a1ee968e9")
set(expected_todo_3 "92b099f26fab59df64720a6f77f9b1cc4fc7507ab843753314290db13bd935a2")

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
