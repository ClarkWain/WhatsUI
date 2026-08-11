@echo off
setlocal EnableExtensions

rem Build and launch the Windows GLFW Focus Tomato app.
rem
rem Usage:
rem     build.bat [Debug|Release] [--clean] [--no-launch]
rem
rem Positional argument sets the CMake configuration (default: Release, since
rem this is the intended interactive demo build). Flags may appear in any
rem order:
rem     --clean       Delete build-focus-tomato\ before configuring
rem                   (forces a full cold build).
rem     --no-launch   Do not `start` the compiled executable at the end.
rem                   Useful for CI, benchmarking, and any wrapper that
rem                   consumes the exit code without expecting a window.

set "CONFIG="
set "CLEAN="
set "NO_LAUNCH="

:parse
if "%~1"=="" goto :parsed
if /I "%~1"=="Debug"       ( set "CONFIG=Debug"      & shift & goto :parse )
if /I "%~1"=="Release"     ( set "CONFIG=Release"    & shift & goto :parse )
if /I "%~1"=="--clean"     ( set "CLEAN=1"           & shift & goto :parse )
if /I "%~1"=="--no-launch" ( set "NO_LAUNCH=1"       & shift & goto :parse )
if /I "%~1"=="-h"          goto :usage
if /I "%~1"=="/h"          goto :usage
if /I "%~1"=="--help"      goto :usage
echo Unknown argument: %~1
goto :usage
:parsed

if "%CONFIG%"=="" set "CONFIG=Release"

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD_DIR=%ROOT%\build-focus-tomato"
set "DEMO=%BUILD_DIR%\examples\%CONFIG%\WhatsUIFocusTomatoApp.exe"

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: CMake was not found on PATH.
    exit /b 1
)

pushd "%ROOT%" >nul

if defined CLEAN (
    echo [0/3] --clean requested; removing %BUILD_DIR%...
    if exist "%BUILD_DIR%" rd /s /q "%BUILD_DIR%"
)

echo [1/3] Updating required submodules...
rem Skip when the WhatsCanvas submodule already looks populated. A one-off
rem `git submodule update` on a freshly cloned repo takes real time; on every
rem subsequent run it costs a couple of seconds for nothing. Users who need
rem to refresh submodules can run `git submodule update --init --recursive`
rem manually.
if exist "%ROOT%\third_party\WhatsCanvas\CMakeLists.txt" (
    echo   third_party\WhatsCanvas already initialised, skipping.
) else (
    git submodule update --init --recursive
    if errorlevel 1 goto :failure
)

echo [2/3] Configuring %CONFIG% Focus Tomato app...
rem Keep the interactive Windows demo on the native DirectWrite path. It is
rem materially sharper for small Fluent UI text than the grayscale FreeType
rem atlas on the supported Windows desktop configuration.
rem Skip the configure step when the build directory has already been
rem generated. CMake's own regeneration rule (ZERO_CHECK) re-runs cmake
rem automatically inside `cmake --build` when any CMakeLists.txt is newer
rem than the cache, so this is safe.
if exist "%BUILD_DIR%\CMakeCache.txt" (
    echo   %BUILD_DIR% already configured, skipping cmake configure.
) else (
    cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_EXAMPLES=ON -DWHATSUI_BUILD_TESTS=OFF -DWHATSUI_ENABLE_ADVANCED_TEXT=ON -DWHATSCANVAS_ENABLE_FREETYPE_RASTERIZER=OFF -DWHATSCANVAS_ENABLE_OPENTYPE_SHAPING=ON
    if errorlevel 1 goto :failure
)

echo [3/3] Building WhatsUIFocusTomatoApp...
rem The GLFW example targets in examples/CMakeLists.txt already attach a
rem PRE_LINK taskkill so a still-running previous instance does not block
rem the re-link with LNK1104.
rem /MP (in root CMakeLists.txt) gives cl.exe per-file parallelism inside
rem one project. --parallel additionally lets msbuild pipeline independent
rem projects. On a 16-core box a cold Focus Tomato build takes ~46s of pure
rem cmake --build time with both, ~84s with /MP alone, and 5-10+ min without
rem either.
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target WhatsUIFocusTomatoApp --parallel
if errorlevel 1 goto :failure

if not exist "%DEMO%" (
    echo Error: expected demo executable was not produced:
    echo %DEMO%
    goto :failure
)

if defined NO_LAUNCH (
    echo Build complete: %DEMO%
    popd >nul
    exit /b 0
)

echo Launching Focus Tomato...
start "WhatsUI Focus Tomato" /D "%BUILD_DIR%\examples\%CONFIG%" "%DEMO%"
set "RESULT=%ERRORLEVEL%"
popd >nul
exit /b %RESULT%

:usage
echo Usage: %~nx0 [Debug^|Release] [--clean] [--no-launch]
echo   Debug^|Release   CMake configuration to build (default: Release)
echo   --clean         Delete build-focus-tomato\ before configuring
echo   --no-launch     Do not start the compiled executable at the end
popd 2>nul >nul
exit /b 2

:failure
set "RESULT=%ERRORLEVEL%"
if "%RESULT%"=="0" set "RESULT=1"
popd >nul
exit /b %RESULT%
