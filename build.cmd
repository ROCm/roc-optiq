@echo off

REM THIS LINE NEEDED TO SUPPORT CRUISECONTROL
SET DEVROOT=%CD%
SET VISUALIZER_FILE_NAME=rocprof-visualizer

if %ERRORLEVEL%==0 ECHO. > %DEVROOT%\success

REM Create build directory
IF NOT EXIST "%DEVROOT%\build" mkdir "%DEVROOT%\build"

REM Run cmake build commands

cmake --preset "x64-release"
cmake --build %DEVROOT%\build\x64-release --preset "Windows Release Build" --parallel 4
IF NOT EXIST "%DEVROOT%\build\x64-release\Release\%VISUALIZER_FILE_NAME%.exe" (
    ECHO ❌ %VISUALIZER_FILE_NAME% was not built!
    GOTO BUILDISSUE
) ELSE (
    ECHO ✅ %VISUALIZER_FILE_NAME% has been successfully built!
)

ECHO Build.cmd: Creating standard release folder structure
CALL %DEVROOT%\CreateReleaseFolderStructure.cmd
if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE

REM Use : "%DEVROOT%\ReleaseCommon\64Bit\All\" - For Public release files , if any
REM Use : "%DEVROOT%\ReleaseInternal\64Bit\All\" - For Internal release files , if any
REM Use : "%DEVROOT%\ReleaseNDA\64Bit\All\" - For NDA release files , if any
 
REM Keep using md to create subfolders if needed.
REM Example : md %DEVROOT%\ReleaseCommon\64Bit\All\bin
REM copy /y "%DEVROOT%\somefile" "%DEVROOT%\ReleaseCommon\64Bit\All\bin"
REM if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE

REM Copy the built executable to proper folder structure for release
ECHO Build.cmd: Copying files to standard folder structure
copy /y "%DEVROOT%\build\x64-release\Release\%VISUALIZER_FILE_NAME%.exe" "%DEVROOT%\ReleaseNDA\64Bit\All\"
REM copy /y "%DEVROOT%\build\x64-release\Release\glfw3.dll" "%DEVROOT%\ReleaseNDA\64Bit\All\"
copy /y "%DEVROOT%\README.md" "%DEVROOT%\ReleaseNDA\64Bit\All\"
md "%DEVROOT%\ReleaseNDA\64Bit\All\doc"
copy /y "%DEVROOT%\doc\ui_sections.png" "%DEVROOT%\ReleaseNDA\64Bit\All\doc\"
if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE

GOTO END

:BUILDISSUE
del /Q /F %DEVROOT%\success
ECHO Build failed!

:END
ECHO Build completed.
