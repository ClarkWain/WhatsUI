include_guard(GLOBAL)

# Enable runtime memory diagnostics for every consumer of the core target.
#
# Sanitizer instrumentation has to be present in both the library and the test
# executable that links it.  Public usage requirements therefore deliberately
# propagate the compiler and linker flags to WhatsUI's in-tree test targets.
# This option is OFF by default so it never changes normal developer or
# release builds.
function(whatsui_enable_sanitizers target)
    if(MSVC)
        # MSVC currently ships AddressSanitizer, but not UndefinedBehavior-
        # Sanitizer.  Use its supported equivalent in the Windows CI job.
        target_compile_options(${target} PUBLIC
            /fsanitize=address
            /Zi
        )
        target_link_options(${target} PUBLIC
            /INCREMENTAL:NO
        )
        message(STATUS "WhatsUI sanitizers: MSVC AddressSanitizer enabled (UBSan is unavailable with MSVC).")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PUBLIC
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target} PUBLIC
            -fsanitize=address,undefined
        )
        message(STATUS "WhatsUI sanitizers: AddressSanitizer and UndefinedBehaviorSanitizer enabled.")
    else()
        message(FATAL_ERROR
            "WHATSUI_ENABLE_SANITIZERS=ON is unsupported for compiler "
            "'${CMAKE_CXX_COMPILER_ID}'. Use MSVC, Clang, or GNU.")
    endif()
endfunction()

# MSVC's ASan runtime is a dynamic DLL.  CTest starts binaries from their
# output directories, which do not normally inherit the Visual Studio tools
# directory on PATH.  Put the matching runtime next to each in-tree test
# executable so local CTest and GitHub Actions behave identically.
function(whatsui_prepare_sanitized_executable target)
    if(NOT MSVC)
        return()
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_whatsui_asan_arch "x86_64")
    else()
        set(_whatsui_asan_arch "i386")
    endif()

    get_filename_component(_whatsui_compiler_directory "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_whatsui_asan_runtime
        "${_whatsui_compiler_directory}/clang_rt.asan_dynamic-${_whatsui_asan_arch}.dll")
    if(NOT EXISTS "${_whatsui_asan_runtime}")
        message(FATAL_ERROR
            "MSVC AddressSanitizer runtime was not found at "
            "'${_whatsui_asan_runtime}'. Install the Visual Studio C++ "
            "AddressSanitizer component or configure with Clang/GCC.")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_whatsui_asan_runtime}" "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
endfunction()
