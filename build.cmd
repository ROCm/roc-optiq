@echo off

REM THIS LINE NEEDED TO SUPPORT CRUISECONTROL
SET DEVROOT=%CD%
SET VISUALIZER_FILE_NAME=rocprof-visualizer

if %ERRORLEVEL%==0 ECHO. > %DEVROOT%\success

REM Installer Reqs
REM ***************
REM %DEVROOT% is a variable used during build, signifying rootfolder in a git repo.

REM === PowerShell translation begins ===

REM Create build directory
IF NOT EXIST "%DEVROOT%\build" mkdir "%DEVROOT%\build"

REM Run cmake build commands
cmake --preset "x64-debug"
cmake --build %DEVROOT%\build\x64-debug
IF NOT EXIST "%DEVROOT%\build\x64-debug\Debug\%VISUALIZER_FILE_NAME%.exe" (
    ECHO ❌ %VISUALIZER_FILE_NAME% was not built!
    GOTO BUILDISSUE
) ELSE (
    ECHO ✅ %VISUALIZER_FILE_NAME% has been successfully built!
)

ECHO Build.cmd: Creating standard release folder structure
CALL %DEVROOT%\CreateReleaseFolderStructure.cmd
if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE

REM Copy the built executable to proper folder structure for release
ECHO Build.cmd: Copying files to standard folder structure
copy /y "%DEVROOT%\build\x64-debug\Debug\%VISUALIZER_FILE_NAME%.exe" "%DEVROOT%\ReleaseCommon\Common\All\"
if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE


:BUILDISSUE
del /Q /F %DEVROOT%\success
ECHO Build failed!
GOTO END

:END
ECHO Build completed.
