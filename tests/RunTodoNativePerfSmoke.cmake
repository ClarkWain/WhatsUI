# Runs the interactive Todo performance smoke with a bounded native process.
#
# CTest itself enforces a small outer timeout too, but keep this watchdog in
# the script so a hung child produces the captured native stdout/stderr rather
# than a generic CTest timeout.  `WhatsUITodoGlfw --perf-smoke` has its own
# shorter in-process watchdog for the no-frame case.

if(NOT DEFINED WHATSUI_TODO_GLFW_EXECUTABLE
   OR NOT EXISTS "${WHATSUI_TODO_GLFW_EXECUTABLE}")
    message(FATAL_ERROR
        "WHATSUI_TODO_GLFW_EXECUTABLE must name a built WhatsUITodoGlfw executable")
endif()

if(NOT DEFINED WHATSUI_TODO_NATIVE_PERF_WORKING_DIRECTORY)
    set(WHATSUI_TODO_NATIVE_PERF_WORKING_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/todo_native_perf_smoke")
endif()
file(MAKE_DIRECTORY "${WHATSUI_TODO_NATIVE_PERF_WORKING_DIRECTORY}")
# The interactive sample persists tasks by design.  A smoke run must begin
# with an empty store, otherwise an earlier successful run can turn its fixed
# composer text into a duplicate and stop exercising the row creation path.
set(native_local_app_data "${WHATSUI_TODO_NATIVE_PERF_WORKING_DIRECTORY}/localappdata")
file(REMOVE_RECURSE "${native_local_app_data}")
file(MAKE_DIRECTORY "${native_local_app_data}")

set(native_timeout_seconds 15)
execute_process(
    # Override only the app's normal store root. This never touches a
    # developer's real LocalAppData Todo store and makes repeated CTest runs
    # start from an empty model.
    COMMAND "${CMAKE_COMMAND}" -E env "LOCALAPPDATA=${native_local_app_data}"
        "${WHATSUI_TODO_GLFW_EXECUTABLE}" --perf-smoke
    WORKING_DIRECTORY "${WHATSUI_TODO_NATIVE_PERF_WORKING_DIRECTORY}"
    TIMEOUT ${native_timeout_seconds}
    RESULT_VARIABLE native_result
    OUTPUT_VARIABLE native_output
    ERROR_VARIABLE native_error
)
set(native_log "${native_output}\n${native_error}")

# A graphical desktop is an explicit prerequisite of this native test.  Do
# not treat an unavailable GLFW/OpenGL surface as a successful smoke run: the
# CTest registration maps this marker to SKIP.
set(native_succeeded FALSE)
if(native_result MATCHES "^[0-9]+$" AND native_result EQUAL 0)
    set(native_succeeded TRUE)
endif()
if(NOT native_succeeded)
    if(native_log MATCHES "glfwInit\\(\\) failed"
       OR native_log MATCHES "glfwCreateWindow\\(\\) failed")
        message(STATUS
            "SKIP: native GLFW/OpenGL surface unavailable; Todo perf smoke was not executed.\n${native_log}")
        return()
    endif()

    if(native_result MATCHES "[Tt]imeout")
        message(FATAL_ERROR
            "Todo native perf smoke exceeded the ${native_timeout_seconds}s process watchdog.\n"
            "Captured output follows:\n${native_log}")
    endif()

    message(FATAL_ERROR
        "Todo native perf smoke exited with ${native_result}.\n"
        "Captured output follows:\n${native_log}")
endif()

foreach(required_marker IN ITEMS
        "Todo perf smoke: started"
        "Todo perf smoke: max update=")
    string(FIND "${native_log}" "${required_marker}" marker_position)
    if(marker_position EQUAL -1)
        message(FATAL_ERROR
            "Todo native perf smoke returned success without required marker '${required_marker}'.\n"
            "Captured output follows:\n${native_log}")
    endif()
endforeach()

# Keep the budgets deliberately conservative: this is a Debug native smoke
# test whose job is to catch multi-frame stalls and regressions, not benchmark
# a particular GPU.  Each interaction gets its own ceiling so a responsive
# radio cannot hide a slow composer submit or task-row rebuild.
function(assert_native_action_latency action_name budget_milliseconds)
    string(REGEX MATCH
        "Todo perf smoke: ${action_name}=ok latency=([0-9]+(\\.[0-9]+)?)ms layout=([0-9]+(\\.[0-9]+)?)ms paint=([0-9]+(\\.[0-9]+)?)ms"
        action_metric
        "${native_log}")
    if(action_metric STREQUAL "")
        message(FATAL_ERROR
            "Todo native perf smoke did not complete '${action_name}' with a parseable metric.\n"
            "Captured output follows:\n${native_log}")
    endif()

    set(latency_milliseconds "${CMAKE_MATCH_1}")
    if(latency_milliseconds GREATER budget_milliseconds)
        message(FATAL_ERROR
            "Todo native perf smoke '${action_name}' took ${latency_milliseconds}ms; "
            "the conservative ${budget_milliseconds}ms budget was exceeded.\n"
            "Captured output follows:\n${native_log}")
    endif()
    message(STATUS
        "Todo native perf smoke '${action_name}' latency=${latency_milliseconds}ms "
        "(budget=${budget_milliseconds}ms)")
endfunction()

assert_native_action_latency("composer-submit" 1200)
assert_native_action_latency("checkbox-toggle" 900)
assert_native_action_latency("radio-select" 900)

message(STATUS "Todo native perf smoke completed successfully:\n${native_log}")
