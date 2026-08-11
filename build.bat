@echo off
setlocal EnableExtensions

rem Build and launch the Windows GLFW Focus Tomato app.
rem Usage: build.bat [Debug^|Release]
rem Release is the default because it is the intended interactive demo build.

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Usage: %~nx0 [Debug^|Release]
    exit /b 2
)

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
rem With /MP set globally in CMakeLists.txt for MSVC, cl.exe already uses
rem every available core to compile files inside one project. Layering
rem msbuild's project-level --parallel on top of /MP oversubscribes the CPU
rem (parallel_project * MP_threads compile processes at once) and, on many
rem machines, ends up slower than plain sequential-projects-with-/MP. So we
rem intentionally do NOT pass --parallel here.
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target WhatsUIFocusTomatoApp
if errorlevel 1 goto :failure

if not exist "%DEMO%" (
    echo Error: expected demo executable was not produced:
    echo %DEMO%
    goto :failure
)

echo Launching Focus Tomato...
start "WhatsUI Focus Tomato" /D "%BUILD_DIR%\examples\%CONFIG%" "%DEMO%"
set "RESULT=%ERRORLEVEL%"
popd >nul
exit /b %RESULT%

:failure
set "RESULT=%ERRORLEVEL%"
if "%RESULT%"=="0" set "RESULT=1"
popd >nul
exit /b %RESULT%
