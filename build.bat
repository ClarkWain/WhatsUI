@echo off
setlocal EnableExtensions

rem Build and launch the Windows GLFW Todo demo.
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
set "BUILD_DIR=%ROOT%\build-todo"
set "DEMO=%BUILD_DIR%\examples\%CONFIG%\WhatsUITodoGlfw.exe"

where cmake >nul 2>nul
if errorlevel 1 (
    echo Error: CMake was not found on PATH.
    exit /b 1
)

pushd "%ROOT%" >nul

echo [1/3] Updating required submodules...
git submodule update --init --recursive
if errorlevel 1 goto :failure

echo [2/3] Configuring %CONFIG% Todo demo...
rem Keep the interactive Windows demo on the native DirectWrite path. It is
rem materially sharper for small Fluent UI text than the grayscale FreeType
rem atlas on the supported Windows desktop configuration.
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DWHATSUI_WITH_WHATSCANVAS=ON -DWHATSUI_BUILD_EXAMPLES=ON -DWHATSUI_BUILD_TESTS=OFF -DWHATSUI_ENABLE_ADVANCED_TEXT=ON -DWHATSCANVAS_ENABLE_FREETYPE_RASTERIZER=OFF -DWHATSCANVAS_ENABLE_OPENTYPE_SHAPING=ON
if errorlevel 1 goto :failure

echo [3/3] Building WhatsUITodoGlfw...
cmake --build "%BUILD_DIR%" --config "%CONFIG%" --target WhatsUITodoGlfw
if errorlevel 1 goto :failure

if not exist "%DEMO%" (
    echo Error: expected demo executable was not produced:
    echo %DEMO%
    goto :failure
)

echo Launching Todo demo...
start "WhatsUI Todo" /D "%BUILD_DIR%\examples\%CONFIG%" "%DEMO%"
set "RESULT=%ERRORLEVEL%"
popd >nul
exit /b %RESULT%

:failure
set "RESULT=%ERRORLEVEL%"
if "%RESULT%"=="0" set "RESULT=1"
popd >nul
exit /b %RESULT%
