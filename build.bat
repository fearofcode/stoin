@echo off
setlocal

set "MODE=%~1"
if "%MODE%"=="" set "MODE=build"

set "BUILD_DIR=build\windows"
set "RELEASE_DIR=%BUILD_DIR%\release"
set "SOURCE_DIR=src"
set "ODIN_BIN=%ODIN%"
if "%ODIN_BIN%"=="" set "ODIN_BIN=odin"
set "RELEASE_FLAGS=%RELEASE_FLAGS%"
if "%RELEASE_FLAGS%"=="" set "RELEASE_FLAGS=-o:speed -disable-assert"

if /I "%MODE%"=="clean" (
    if exist "%BUILD_DIR%" rmdir /S /Q "%BUILD_DIR%"
    exit /B 0
)

"%ODIN_BIN%" version >NUL 2>NUL
if errorlevel 1 (
    echo stoin: odin was not found. Install Odin or set ODIN to the compiler path.
    exit /B 1
)

if /I "%MODE%"=="release" (
    if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"
    "%ODIN_BIN%" build "%SOURCE_DIR%" %RELEASE_FLAGS% -out:"%RELEASE_DIR%\stoin.exe"
    exit /B %ERRORLEVEL%
)

if /I "%MODE%"=="test" (
    "%ODIN_BIN%" test "%SOURCE_DIR%"
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

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
"%ODIN_BIN%" build "%SOURCE_DIR%" -out:"%BUILD_DIR%\stoin.exe"
exit /B %ERRORLEVEL%
