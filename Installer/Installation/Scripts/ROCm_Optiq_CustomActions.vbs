'**************************************************************************************
'This file includes VB Script custom actions written for AMD ROCm™ Optiq product..
'**************************************************************************************
Const ForAppending = 8, ForWriting = 2, ForReading = 1
Const TristateFalse = 0, TristateTrue = 1, TristateUseDefault = 2

'*****************************************************************
' Install Log File
'*****************************************************************
Sub iLogMessage(message)
	Dim objShell, logFolderName, logFileName, logFilePath
	Set objShell = CreateObject("WScript.Shell")					'Prepare log file name
	logFolderName = objShell.ExpandEnvironmentStrings("%USERPROFILE%")
	logFileName = "\ROCm-Optiq-Beta_Install.log"
	logFilePath = logFolderName & logFileName	
	
	Dim objFSO, objTextFile, currentDate
	Set objFSO = CreateObject("Scripting.FileSystemObject")		' Create the File System Object
	Set objTextFile = objFSO.OpenTextFile(logFilePath, ForAppending, True)
	
	currentDate = Now
	' Write message to the log file now 
	objTextFile.WriteLine(FormatDateTime(CurrentDate, vbGeneralDate) & " :: " & message)
	objTextFile.Close
	
	Set objTextFile = Nothing
	Set objFSO = Nothing
	Set objShell = Nothing
	
End Sub

'***********************************************************************
'Function to update security permission for custom installation folder.
'***********************************************************************
Function SetFolPermission()

	Set objShell = CreateObject("WScript.Shell")
	Set objFSO = CreateObject("Scripting.FileSystemObject")
	Dim StrDir : StrDir = objShell.ExpandEnvironmentStrings("%SYSTEMROOT%")
	Dim SysDrv : SysDrv = objShell.ExpandEnvironmentStrings("%SYSTEMDRIVE%")
	
	strCustomActionData = Session.Property("CustomActionData")
	strCustomArray = Split(strCustomActionData,";")
	strInst = strCustomArray(0)

	
	Dim ParentFol : ParentFol = strInst
	ParentFol_up = Left(ParentFol, InStrRev(ParentFol, "\") - 1)
	Dim Svcexe : Svcexe = strInst & "roc-optiq.exe"
	Dim TakeOwnSvcexe : TakeOwnSvcexe = chr(34) & StrDir & "\System32\takeown.exe" & chr(34) & " /a /f " & chr(34) & Svcexe & chr(34)
	
	If Instr(strInst, "\Program Files\") > 0 OR Instr(strInst, "\Program Files (x86)\") > 0 Then
		Call iLogMessage ("SetFolPermission : Not enabling as programfiles detected")
	Else
		Call iLogMessage ("SetFolPermission : Enabling on " & strInst)
		Set objRootFolder = objFSO.GetFolder(strInst)
		Set objRootSubFolder = objRootFolder.SubFolders
		For Each subFolder In objRootSubFolder
			SubFolderPath = subFolder.Path
			SetOwner = chr(34) & StrDir & "\System32\icacls.exe" & chr(34) & chr(32) & chr(34) & subFolder.Path & chr(34) & " /setowner *S-1-5-32-544 /t"
			TakeOwnRec = chr(34) & StrDir & "\System32\takeown.exe" & chr(34) & " /a /r /f " & chr(34) & subFolder.Path & "\*" & chr(34) & " /D N"
			Call iLogMessage ("SetFolPermission : SetOwner " & SetOwner)
			errReturn = objShell.Run (SetOwner,0, True)
			Call iLogMessage("SetFolPermission : SetOwner errReturn : " & errReturn)
			If errReturn <> 0 Then
				objShell.Run TakeOwnRec,0, True
				Call iLogMessage ("SetFolPermission : TakeOwnRec : " & TakeOwnRec)
			End If
		Next

		objShell.Run TakeOwnSvcexe,0, True
			
		Dim SetPermAll : SetPermAll = chr(34) & StrDir & "\System32\icacls.exe" & chr(34) & chr(32) & chr(34) & ParentFol & "*" & chr(34) & " /inheritance:r /grant:r *S-1-5-18:F *S-1-5-32-544:F *BU:RX /c /t /l /q"
		Dim SetPermParent : SetPermParent = chr(34) & StrDir & "\System32\icacls.exe" & chr(34) & chr(32) & chr(34) & ParentFol_up & chr(34) & " /inheritance:r /grant:r *S-1-5-18:F *S-1-5-32-544:F *BU:RX"
		Dim SetPermCLIexe : SetPermCLIexe = chr(34) & StrDir & "\System32\icacls.exe" & chr(34) & chr(32) & chr(34) & Svcexe & chr(34) & " /inheritance:r /grant:r *S-1-5-18:F *S-1-5-32-544:F *BU:RX"
		
		errReturn = objShell.Run (SetPermAll,0, True)
		Call iLogMessage("SetFolPermission : SetPermAll errReturn : " & errReturn)
		errReturn = objShell.Run (SetPermParent,0, True)
		Call iLogMessage("SetFolPermission : SetPermParent errReturn : " & errReturn)
		errReturn = objShell.Run (SetPermCLIexe,0, True)
		Call iLogMessage("SetFolPermission : SetPermCLIexe errReturn : " & errReturn)
		
	End If
	
End Function


'***********************************************************************************************************************
'Function to redirect installation directory always to ROCm™ Optiq-Beta subdirectory in case of custom installation.
'***********************************************************************************************************************
Function Redirect_INSTALLDIR()

	Set objFSO = CreateObject("Scripting.FileSystemObject")
	Dim strInstallPath : strInstallPath = Session.Property("INSTALLDIR")
	'Dim strInstallPathVal : strInstallPathVal = Split(strInstallPath,"\")
	'Dim Instdir : Instdir = strInstallPathVal(ubound(strInstallPathVal)-1)
	Dim Instdir : Instdir = Right(strInstallPath, 1)
	Call iLogMessage("Redirect_INSTALLDIR : Installation directory obtained : " & strInstallPath)
	
	If Instr(strInstallPath, "\Program Files\") > 0 OR Instr(strInstallPath, "\Program Files (x86)\") > 0 Then
		Call iLogMessage("Redirect_INSTALLDIR : Not updating installation directory.")
	Else
		If StrComp(UCase(Instdir), "\") <> 0 Then	
			strInstallPath = strInstallPath & "\"
		End If 
		
		Session.Property("INSTALLDIR") = strInstallPath & "ROCm" & ChrW(&H2122) & " Optiq\"
		Call iLogMessage("Redirect_INSTALLDIR : Installation directory updated")
	End If

End Function