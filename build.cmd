REM THIS LINE NEEDED TO SUPPORT CRUISECONTROL
SET DEVROOT=%CD%
if %ERRORLEVEL%==0 ECHO. > %DevRoot%\success

REM	Installer Reqs
REM ***************
REM %DEVROOT% is a variable used during build, signifying rootfolder in a git repo.
ECHO Build.cmd: Creating standard release folder structure
CALL %DEVROOT%\CreateReleaseFolderStructure.cmd
if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE


REM Whatever files or folders copied to this folder during build will be picked up by installer and deployed on user's system exactly as it is.
REM Use "%DEVROOT%\ReleaseCommon\Common\All\" - For Public release files , if any
REM Use "%DEVROOT%\ReleaseInternal\Common\All\" - For Internal release files , if any
REM Use "%DEVROOT%\ReleaseNDA\Common\All\" - For NDA release files , if any
Echo Build.cmd: Copying files to standard folder structure
copy /y "%DEVROOT%\"name and path in repo where the exe is present" "%DEVROOT%\ReleaseCommon\Common\All\"
if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE
REM Keep using md to create subfolders if needed.
REM Example : md %DEVROOT%\ReleaseCommon\Common\All\bin
REM copy /y "%DEVROOT%\somefile" "%DEVROOT%\ReleaseCommon\Common\All\bin"
REM if %ERRORLEVEL% NEQ 0 GOTO BUILDISSUE

:BUILDISSUE
del /Q /F %DevRoot%\success

:END