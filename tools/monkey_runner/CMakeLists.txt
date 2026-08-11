add_executable(WhatsUIMonkeyRunner main.cpp)
target_compile_features(WhatsUIMonkeyRunner PRIVATE cxx_std_17)
target_link_libraries(WhatsUIMonkeyRunner PRIVATE Dbghelp User32)
if(MSVC)
    target_compile_definitions(WhatsUIMonkeyRunner PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Smoke target: a plain Win32 window that crashes on the first left click,
# used to verify the runner's crash-capture pipeline end-to-end.
add_executable(WhatsUIMonkeyCrashBait crash_bait.cpp)
target_compile_features(WhatsUIMonkeyCrashBait PRIVATE cxx_std_17)
set_target_properties(WhatsUIMonkeyCrashBait PROPERTIES
    OUTPUT_NAME "crash_bait")
