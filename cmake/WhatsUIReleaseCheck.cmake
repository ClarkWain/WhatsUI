# Invoke from a committed checkout, for example:
# cmake -DWHATSUI_SOURCE_DIR=$PWD -DWHATSUI_RELEASE_OUTPUT_DIR=$PWD/release-check -P cmake/WhatsUIReleaseCheck.cmake
#
# This verifies the releasable *headless* developer-preview package only. It
# intentionally does not claim that the in-tree WhatsCanvas/GLFW integration
# is distributable; that M5 package/export work remains open.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED WHATSUI_SOURCE_DIR OR WHATSUI_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "WHATSUI_SOURCE_DIR must name the repository root")
endif()
if(NOT DEFINED WHATSUI_RELEASE_OUTPUT_DIR OR WHATSUI_RELEASE_OUTPUT_DIR STREQUAL "")
    message(FATAL_ERROR "WHATSUI_RELEASE_OUTPUT_DIR must name an empty/throwaway output directory")
endif()

get_filename_component(_source "${WHATSUI_SOURCE_DIR}" ABSOLUTE)
get_filename_component(_output "${WHATSUI_RELEASE_OUTPUT_DIR}" ABSOLUTE)
if(NOT EXISTS "${_source}/CMakeLists.txt" OR NOT EXISTS "${_source}/tests/package_consumer/CMakeLists.txt")
    message(FATAL_ERROR "WHATSUI_SOURCE_DIR does not look like a WhatsUI source checkout")
endif()

find_program(_git git REQUIRED)
execute_process(COMMAND "${_git}" -C "${_source}" status --porcelain
                RESULT_VARIABLE _git_status_result OUTPUT_VARIABLE _git_status ERROR_VARIABLE _git_status_error)
if(NOT _git_status_result EQUAL 0)
    message(FATAL_ERROR "Could not inspect Git worktree: ${_git_status_error}")
endif()
if(NOT _git_status STREQUAL "")
    message(FATAL_ERROR "Release check requires a clean committed worktree. Commit/stash changes first:\n${_git_status}")
endif()

# These files are release metadata gates, not a legal interpretation. The
# check deliberately fails until the release owner supplies and approves every
# required artifact. ALLOW_MISSING_METADATA is only for rehearsal builds.
set(_required_metadata
    "LICENSE"
    "NOTICE"
    "CHANGELOG.md"
    "doc/whatsui/SBOM.md"
    "doc/whatsui/RELEASE_CHECKLIST.md"
    "doc/whatsui/WINDOWS_SUPPORT_MATRIX.md"
    "doc/whatsui/UPGRADE_AND_CONTRIBUTING.md")
set(_missing_metadata)
foreach(_metadata IN LISTS _required_metadata)
    if(NOT EXISTS "${_source}/${_metadata}")
        list(APPEND _missing_metadata "${_metadata}")
    endif()
endforeach()
if(_missing_metadata AND NOT WHATSUI_RELEASE_ALLOW_MISSING_METADATA)
    message(FATAL_ERROR "Release metadata is incomplete: ${_missing_metadata}. See doc/whatsui/RELEASE_CHECKLIST.md")
elseif(_missing_metadata)
    message(WARNING "Rehearsal only: missing release metadata: ${_missing_metadata}")
endif()

file(MAKE_DIRECTORY "${_output}")
set(_archive "${_output}/whatsui-source.zip")
execute_process(COMMAND "${_git}" -C "${_source}" archive --format=zip --output="${_archive}" HEAD
                RESULT_VARIABLE _archive_result ERROR_VARIABLE _archive_error)
if(NOT _archive_result EQUAL 0)
    message(FATAL_ERROR "Could not create clean Git source archive: ${_archive_error}")
endif()
file(SHA256 "${_archive}" _archive_sha256)
execute_process(COMMAND "${_git}" -C "${_source}" rev-parse HEAD
                RESULT_VARIABLE _revision_result OUTPUT_VARIABLE _revision OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _revision_result EQUAL 0)
    message(FATAL_ERROR "Could not determine release revision")
endif()
execute_process(COMMAND "${_git}" -C "${_source}" submodule status --recursive
                RESULT_VARIABLE _submodule_result OUTPUT_VARIABLE _submodules OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _submodule_result EQUAL 0)
    message(FATAL_ERROR "Could not record submodule revisions")
endif()
file(WRITE "${_output}/SOURCE_MANIFEST.txt"
    "WhatsUI source revision: ${_revision}\n"
    "Archive: ${_archive}\n"
    "SHA-256: ${_archive_sha256}\n"
    "Submodules (must be initialized by source-package consumer):\n${_submodules}\n")

# Verify the only currently exported distribution: the headless core package.
set(_package_build "${_output}/headless-package-build")
set(_install_prefix "${_output}/headless-install")
set(_consumer_build "${_output}/headless-consumer-build")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${_source}" -B "${_package_build}"
                        -DWHATSUI_WITH_WHATSCANVAS=OFF
                        -DWHATSUI_BUILD_TESTS=OFF
                        -DWHATSUI_BUILD_EXAMPLES=OFF
                        -DWHATSUI_INSTALL=ON
                        "-DCMAKE_INSTALL_PREFIX=${_install_prefix}"
                RESULT_VARIABLE _configure_result)
if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR "Headless package configure failed")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${_package_build}" --config Release
                RESULT_VARIABLE _build_result)
if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR "Headless package build failed")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${_package_build}" --config Release
                RESULT_VARIABLE _install_result)
if(NOT _install_result EQUAL 0)
    message(FATAL_ERROR "Headless package install failed")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${_source}/tests/package_consumer" -B "${_consumer_build}"
                        "-DCMAKE_PREFIX_PATH=${_install_prefix}"
                RESULT_VARIABLE _consumer_configure_result)
if(NOT _consumer_configure_result EQUAL 0)
    message(FATAL_ERROR "External headless consumer configure failed")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${_consumer_build}" --config Release
                RESULT_VARIABLE _consumer_build_result)
if(NOT _consumer_build_result EQUAL 0)
    message(FATAL_ERROR "External headless consumer build failed")
endif()
file(GLOB_RECURSE _consumer_candidates "${_consumer_build}/whatsui_consumer_smoke*" )
list(FILTER _consumer_candidates EXCLUDE REGEX "\\.(cmake|vcxproj|filters|obj|pdb|ilk)$")
list(LENGTH _consumer_candidates _consumer_count)
if(_consumer_count EQUAL 0)
    message(FATAL_ERROR "External consumer executable was not found")
endif()
list(GET _consumer_candidates 0 _consumer_executable)
execute_process(COMMAND "${_consumer_executable}" RESULT_VARIABLE _consumer_run_result)
if(NOT _consumer_run_result EQUAL 0)
    message(FATAL_ERROR "External headless consumer executable failed: ${_consumer_run_result}")
endif()

message(STATUS "Release groundwork passed for headless core only")
message(STATUS "Source archive SHA-256: ${_archive_sha256}")
message(STATUS "Open M5 boundary: WhatsCanvas/GLFW package export is not checked or released by this script")
