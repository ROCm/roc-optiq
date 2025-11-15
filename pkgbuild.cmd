REM PkgBuild.cmd
REM =========
REM MAKES AN INSTALLSHIELD PACKAGE
REM DEPENDS ON STANDARD AMD INSTALLSHIELD PROJECT STRUCTURE AND IS PATH RELATIVE

SET CMDROOTDIRECTORY=c:\Windows\System32
IF NOT EXIST c:\Windows\SysWOW64 GOTO SKIP64BIT
SET CMDROOTDIRECTORY=c:\Windows\SysWOW64
:SKIP64BIT
ECHO %CMDROOTDIRECTORY%

REM use clrenv.cmd to debug clean up before starting
call D:\batch\clrenv.cmd

:SETUP_PARAMETERS
REM PACKAGE DEVELOPER SHOULD SET PROJECT_NAME AND SCRIPT_NAME
REM AS PkgBuild.cmd IS MIGRATED TO NEW PROJECTS

SET PKG_SCRIPT_PATH="Installation\Scripts"
SET SCRIPT_NAME=setup.ism
SET PKGCHECK=TRUE
SET ISM_FILE_PATH=%DEVROOT%\Installation\Scripts\setup.ism
SET ROOTFOLDER=%DEVROOT%


echo Copying Installer folder to root for RMS build...
xcopy "%DEVROOT%\Installer" "%DEVROOT%" /E /I /Y


Echo PkgBuild.cmd: ISM File Path %ISM_FILE_PATH%
Echo PkgBuild.cmd: Root Folder %DEVROOT%

:PICK RELEASETYPE
if %theBType%=="alpha" GOTO ALPHASET
if %theBType%=="beta" GOTO BETASET
if %theBType%=="test" GOTO TESTSET
if %theBType%=="release" GOTO RELEASESET

:ALPHASET
SET PROJECT_NAME=ROCm-Optiq-Beta Alpha
GOTO CONTINUE

:BETASET
SET PROJECT_NAME=ROCm-Optiq-Beta Beta
GOTO CONTINUE

:TESTSET
SET PROJECT_NAME=ROCm-Optiq-Beta NOT FOR DISTRIBUTION
GOTO CONTINUE

:RELEASESET
SET PROJECT_NAME=ROCm-Optiq-Beta
GOTO CONTINUE

:CONTINUE

REM AMDVersion.cmd must be dropped by Version.pl
call AMDVersion.cmd
SET PKG_VERSION
call CreateReleaseFolderStructure.cmd

mkdir BOM
mkdir BOM\Internal
mkdir BOM\NDA
mkdir BOM\Public

REM Don't change these! 
REM ==================
SET PKG_SCRIPT_PATH="Installation\Scripts"
pushd %PKG_SCRIPT_PATH%

:Update_Copyright_Year
	Echo PkgBuild.cmd: Updating copyright info
	For /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set myyear=%%c)
	Echo PkgBuild.cmd : Current year obtained is %myyear%
	%CMDROOTDIRECTORY%\wscript.exe Update_Copyright.vbs %myyear%
	if %ERRORLEVEL%==0 ECHO. > %DevRoot%\pkgsuccess

:Update_Version_Number
Echo PkgBuild.cmd: Updating Version info
%CMDROOTDIRECTORY%\wscript.exe Update_Version.vbs
if %ERRORLEVEL%==0 ECHO. > %DevRoot%\pkgsuccess

:Installer_Automation
Echo PkgBuild.cmd: Including files through automated way.
%CMDROOTDIRECTORY%\cscript InstallerAutomation.vbs
if %ERRORLEVEL% NEQ 0 del /Q /F %DevRoot%\pkgsuccess

REM :Build_Package
REM Echo PkgBuild.cmd: Building %PKG_SCRIPT_PATH%\%SCRIPT_NAME% Public
REM "C:\Program Files (x86)\InstallShield\2023 SAB\System\IsCmdBld.exe" -p %SCRIPT_NAME% -b "%DEVROOT%\Temp\Public" -r "Public_Compressed" -licCheckTimeOut 1999
REM if %ERRORLEVEL% NEQ 0 del /Q /F %DevRoot%\pkgsuccess

REM :POST_PROCESS
REM Echo PkgBuild.cmd: Copy Public release into BOM folder
REM copy /y "%DEVROOT%\Temp\Public\ROCm-Visualizer-Beta.exe" "%DEVROOT%\Bom\Public\ROCm-Visualizer-Beta.exe"
REM Echo PkgBuild.cmd: Error is %ERRORLEVEL%
REM if %ERRORLEVEL% NEQ 0 del /Q /F %DevRoot%\pkgsuccess

:Build_Package
Echo PkgBuild.cmd: Building %PKG_SCRIPT_PATH%\%SCRIPT_NAME% Internal
"C:\Program Files (x86)\InstallShield\2023 SAB\System\IsCmdBld.exe" -p %SCRIPT_NAME% -b "%DEVROOT%\Temp\Internal" -r "Internal_Compressed" -licCheckTimeOut 1999
if %ERRORLEVEL% NEQ 0 del /Q /F %DevRoot%\pkgsuccess

:POST_PROCESS
Echo PkgBuild.cmd: Copy Internal release into BOM folder
copy /y "%DEVROOT%\Temp\Internal\ROCm-Optiq.exe" "%DEVROOT%\Bom\Internal\ROCm-Optiq.exe"
Echo PkgBuild.cmd: Error is %ERRORLEVEL%
if %ERRORLEVEL% NEQ 0 del /Q /F %DevRoot%\pkgsuccess

:Build_Package
Echo PkgBuild.cmd: Building %PKG_SCRIPT_PATH%\%SCRIPT_NAME% NDA
"C:\Program Files (x86)\InstallShield\2023 SAB\System\IsCmdBld.exe" -p %SCRIPT_NAME% -b "%DEVROOT%\Temp\NDA" -r "NDA_Compressed" -licCheckTimeOut 1999
if %ERRORLEVEL% NEQ 0 del /Q /F %DevRoot%\pkgsuccess

:POST_PROCESS
Echo PkgBuild.cmd: Copy NDA release into BOM folder
copy /y "%DEVROOT%\Temp\NDA\ROCm-Optiq.exe" "%DEVROOT%\Bom\NDA\ROCm-Optiq.exe"
Echo PkgBuild.cmd: Error is %ERRORLEVEL%
if %ERRORLEVEL% NEQ 0 del /Q /F %DevRoot%\pkgsuccess

