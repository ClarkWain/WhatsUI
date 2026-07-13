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
set(expected_todo_0 "10af91af2df14ddaaa028625926e3f0c8a3805ffbc07bf5505fc82fe8fb1a91a")
set(expected_todo_1 "027412ac40379e8e82c73f65f4851d6c1492a8dd1bef1cc8912d3a7a25c3c56f")
set(expected_todo_2 "07792e967f7a6687ce2be4c5fc821374c1743984057f849205db95abd5c1fe6f")
set(expected_todo_3 "10af91af2df14ddaaa028625926e3f0c8a3805ffbc07bf5505fc82fe8fb1a91a")

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
