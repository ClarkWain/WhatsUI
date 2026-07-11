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
set(expected_todo_0 "4dd8c5d0d0000b9d8e718f879f20a87aaf6e7e58867766eefc122c474d967378")
set(expected_todo_1 "b93ec5d96b9b3ade87d269b907de8e6dbb148ddb271302e12b0cdd82a259fcae")
set(expected_todo_2 "ef6143c0a0174b15a1b4b195f66a48de26ac3b24889db93a790859a2bdbfe7c3")
set(expected_todo_3 "4dd8c5d0d0000b9d8e718f879f20a87aaf6e7e58867766eefc122c474d967378")

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
