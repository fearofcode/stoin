@echo off
setlocal

set "MODE=%~1"
if "%MODE%"=="" set "MODE=build"

if /I "%MODE%"=="clean" (
    if exist build\windows rmdir /S /Q build\windows
    exit /B 0
)

where cl >NUL 2>NUL
if errorlevel 1 (
    echo stoin: cl.exe was not found. Run this from a Visual Studio Developer Command Prompt.
    exit /B 1
)

set "BUILD_DIR=build\windows"
set "OBJ_DIR=%BUILD_DIR%\obj"
set "TEST_OBJ_DIR=%BUILD_DIR%\test_obj"
set "RELEASE_OBJ_DIR=%BUILD_DIR%\release_obj"

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"
if not exist "%TEST_OBJ_DIR%" mkdir "%TEST_OBJ_DIR%"
if not exist "%RELEASE_OBJ_DIR%" mkdir "%RELEASE_OBJ_DIR%"

set "COMMON_SOURCES=src\dictionary.c src\dictionary_stack.c src\format.c src\gemini_pr.c src\keymap.c src\orthography.c src\phrasing.c src\raw_serial.c src\retro.c src\runtime_config.c src\stentura.c src\steno.c src\steno_stroke.c src\stroke_merge.c src\stb_ds_impl.c src\stitch.c src\text_util.c src\translation_history.c src\translation_match.c src\tx_bolt.c src\tx_bolt_multiple.c src\util.c src\platform_windows.c src\platform_windows_atomic.c third_party\cjson\cJSON.c"
set "BASE_CFLAGS=/nologo /TC /std:c11 /W3 /D_CRT_SECURE_NO_WARNINGS /D_WIN32_WINNT=0x0601 /I src"
set "LDFLAGS=user32.lib setupapi.lib advapi32.lib"

if /I "%MODE%"=="release" (
    cl %BASE_CFLAGS% /O2 /DNDEBUG /Fe"%BUILD_DIR%\stoin.exe" /Fo"%RELEASE_OBJ_DIR%\\" src\main.c %COMMON_SOURCES% /link %LDFLAGS%
    if errorlevel 1 exit /B 1
    exit /B 0
)

if /I "%MODE%"=="test" (
    cl %BASE_CFLAGS% /Zi /Fe"%BUILD_DIR%\test_steno.exe" /Fo"%TEST_OBJ_DIR%\\" tests\test_steno.c %COMMON_SOURCES% /link %LDFLAGS%
    if errorlevel 1 exit /B 1
    "%BUILD_DIR%\test_steno.exe"
    if errorlevel 1 exit /B 1
    where go >NUL 2>NUL
    if not errorlevel 1 (
        go test ./...
        if errorlevel 1 exit /B 1
    ) else (
        echo stoin: go.exe not found; skipped Go tests.
    )
    exit /B 0
)

if /I not "%MODE%"=="build" (
    echo usage: build.bat [build^|test^|release^|clean]
    exit /B 2
)

cl %BASE_CFLAGS% /Zi /Fe"%BUILD_DIR%\stoin.exe" /Fo"%OBJ_DIR%\\" src\main.c %COMMON_SOURCES% /link %LDFLAGS%
exit /B %ERRORLEVEL%
