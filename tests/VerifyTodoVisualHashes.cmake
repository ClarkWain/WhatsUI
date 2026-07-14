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
set(expected_todo_0 "d3f02baeca5ed620bfa25ffd68f6d0f13b47b2f34774249ed0e7f73ac55d5c60")
set(expected_todo_1 "ba43a8534795ab57d11b81400309f9b1d919898b1c00a9dee0f3b6383e17f6fa")
set(expected_todo_2 "7d41eee0e656bd8fe92be7c4fbc37997951b15a5ddbd26979d044c948cb50d0e")
set(expected_todo_3 "d3f02baeca5ed620bfa25ffd68f6d0f13b47b2f34774249ed0e7f73ac55d5c60")

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
