Option Explicit
'********************************************************
'Main
'**************************************************
'Variable declaration section.
Dim ISMFILEPATH, bitType, releaseType, releaseFlag, sourceFolder, strBitPosition, rootSourceFolder, subFolder, rootFolderName
Dim objFSO, objRootFolder, objRootSubFolder, objRootConditionFolder, WshShell, objStdOut, objShell, objProjectFile
Dim strIntermediateFolder, strIntPosition, updatedComponentCondition, conditionalFolder, rootFolder, conditionalSubFolder
Dim strTotalComponentCount :: strTotalComponentCount = 0
Dim strFileComponentCount :: strFileComponentCount = 0
Dim strFolderComponentCount :: strFolderComponentCount = 0
Dim strScriptStartTime, strScriptEndTime, strTotalTimeTaken, strAverageTime

Set objFSO = CreateObject("Scripting.FileSystemObject")
Set objShell = WScript.CreateObject("WScript.Shell")


'Setting up log object
Set objStdOut = WScript.StdOut
objStdOut.WriteLine Now & "  - Installer automation script started."
strScriptStartTime = Time

'Create the WScript object to read Environment Variables later
Set WshShell = CreateObject("WScript.Shell")

'Reading Environment Variables
ISMFILEPATH = WshShell.ExpandEnvironmentStrings("%ISM_FILE_PATH%")
rootSourceFolder = WshShell.ExpandEnvironmentStrings("%ROOTFOLDER%")

'Setting up ISM project object initialization area.
Dim ismFile :: ismFile = ISMFILEPATH
Set objProjectFile = CreateObject ("IswiAuto29.ISWiProject")
objProjectFile.OpenProject ismFile

Set objRootFolder = objFSO.GetFolder(rootSourceFolder)
Set objRootSubFolder = objRootFolder.SubFolders

For Each subFolder In objRootSubFolder
	rootFolder = subFolder.Path
	rootFolderName = subFolder.Name
	If ((StrComp(rootFolderName,"ReleaseInternal")=0) OR (StrComp(rootFolderName,"ReleaseNDA")=0) OR (StrComp(rootFolderName,"ReleaseCommon")=0) OR (StrComp(rootFolderName,"ReleasePublic")=0)) Then
		releaseFlag = SetReleaseFlag(rootFolder)
		releaseType = SetReleaseType(releaseFlag)
		Set objRootConditionFolder = subFolder.SubFolders
		For Each conditionalSubFolder In objRootConditionFolder
			conditionalFolder = conditionalSubFolder.Path
			bitType = SetBitType(conditionalFolder)
			Call SetReleaseComponents(conditionalFolder)
		Next
	Else
		'No action required, since they are not the release folders.
	End If
Next

'Logging part -- Script execution time, total count on components added.
strTotalComponentCount = strFileComponentCount + strFolderComponentCount
objStdOut.WriteLine Now & "  - Installer automation script completed." 
objStdOut.WriteLine vbCrLf
objStdOut.WriteLine "Installer automation script execution summary."
strScriptEndTime = Time
strTotalTimeTaken = CallTimeSeconds(strScriptStartTime, strScriptEndTime)
objStdOut.WriteLine "Total number of components added for setup file is " & strTotalComponentCount & " in duration of "  & strTotalTimeTaken & " seconds."
strAverageTime = (strTotalTimeTaken / strTotalComponentCount)
objStdOut.WriteLine "Average time taken for adding each component is " & strAverageTime & " seconds."
objStdOut.WriteLine vbCrLf

'Avoiding memory leakage.
Set objFSO = Nothing
Set objProjectFile = Nothing

'***********************************************************
'Setting up BitType
'***********************************************************
Function SetBitType(conditionalFolder)

	Dim strBitType :: strBitType = ""
	Dim strBitTypeLength :: strBitTypeLength = ""
	strBitType = (InStrRev(Trim(conditionalFolder),"\"))
	strBitTypeLength = Len(conditionalFolder)-strBitType
	strBitType = Right(conditionalFolder,strBitTypeLength)
	SetBitType = strBitType

End Function
'***********************************************************
'Function which iterated through all folder under release type.
'Internally initiates component add functions.
'***********************************************************
Function SetReleaseComponents(conditionalFolder)

	Dim objFolder, objSubFolder
	Set objFolder = objFSO.GetFolder(conditionalFolder)
	Set objSubFolder = objFolder.SubFolders

	For Each subFolder in objSubFolder
		sourceFolder = subFolder.Path
		strBitPosition = (InStr(Trim(sourceFolder),bitType))  
		strBitPosition = (Len(sourceFolder)-(strBitPosition-1))
		strIntermediateFolder = SetIntermediateFolder(sourceFolder)
		If (bitType = "32Bit" OR bitType = "64Bit") Then
			updatedComponentCondition = SetComponentCondtion(strIntermediateFolder)
		Else
			updatedComponentCondition = ""
		End If
		Call FilesListing(sourceFolder)
		sourceFolder = ""
	Next

	Set objFolder = Nothing
	Set objSubFolder = Nothing

End Function
'***********************************************************
'Setting up ReleaseFlag
'***********************************************************
Function SetReleaseFlag(rootFolder)

	Dim strReleaseFlag :: strReleaseFlag = ""
	Dim releaseFlagLength :: releaseFlagLength = ""
	strReleaseFlag = (InStrRev(Trim(rootFolder),"\"))
	releaseFlagLength = Len(rootFolder)-strReleaseFlag
	strReleaseFlag = Right(rootFolder,releaseFlagLength)
	strReleaseFlag = Trim(Replace(strReleaseFlag, "Release", " ", 1, -1, 1))
	SetReleaseFlag = strReleaseFlag

End Function
'***********************************************************
'Setting up ReleaseType
'***********************************************************
Function SetReleaseType(releaseFlag)

	Dim strReleaseType :: strReleaseType = ""
	If (StrComp(releaseFlag, "Internal") = 0) Then
		strReleaseType = "Application_Internal"
	ElseIf (StrComp(releaseFlag, "NDA") = 0) Then
		strReleaseType = "Application_NDA"
	ElseIf (StrComp(releaseFlag, "Public") = 0) Then
		strReleaseType = "Application_Public"
	ElseIf (StrComp(releaseFlag, "Common") = 0) Then
		strReleaseType = "Application_Common"
	End If
	SetReleaseType = strReleaseType

End Function
'***********************************************************
'Setting up bit position for intermediate folders.
'***********************************************************
Function SetIntermediateFolder(sourceFolder)
	Dim strIntermediateFolder :: strIntermediateFolder = ""
	strIntermediateFolder = Right(Trim(sourceFolder),strBitPosition)
	strIntPosition = (Instr(Trim(strIntermediateFolder),"\"))
	strIntPosition = (Len(strIntermediateFolder)-strIntPosition)
	strIntermediateFolder = Right(Trim(strIntermediateFolder),strIntPosition)
	strIntermediateFolder = Trim(strIntermediateFolder)
	SetIntermediateFolder = strIntermediateFolder
End Function

'***********************************************************
'Setting condition for a component depending upon bittype, type of OS
'***********************************************************
Function SetComponentCondtion(strIntermediateFolder)

	Dim componentCondition :: componentCondition = ""
	If bitType = "32Bit" Then
		componentCondition = "NOT VersionNT64"
	Else bitType = "64Bit"
		componentCondition = "VersionNT64"
	End If	

	Select Case strIntermediateFolder
		Case "All"
			componentCondition = componentCondition
		Case "XP"
			componentCondition = "((" + componentCondition + ")" + " AND " + "(VersionNT = 501))"
		Case "Vista"
			componentCondition = "((" + componentCondition + ")" + " AND " + "(VersionNT = 600))"
		Case 2003
			componentCondition = "((" + componentCondition + ")" + " AND " + "(VersionNT = 502))"
		Case 2008
			componentCondition = "((" + componentCondition + ")" + " AND " + "(VersionNT = 600))"
		Case 7
			componentCondition = "((" + componentCondition + ")" + " AND " + "(VersionNT = 601))"
		Case Else
			'MsgBox "Select Nothing"	   
	End Select
	SetComponentCondtion = componentCondition

End Function
'***********************************************************
'List all files till n-th directory
'Adds each file as an component in *.ism file through "AddComponents" function.
'***********************************************************
Function FilesListing(sourceFolder)

	Dim objFSO, objFolder, colFiles, objFile, folderName, updatedFileName
	Dim fileName, filePath, componentFolderName
	Set objFSO = CreateObject("Scripting.FileSystemObject")
	Set objFolder = objFSO.GetFolder(sourceFolder)
	Set colFiles = objFolder.Files
	componentFolderName = objFolder.Name

	'Lists and adds files as an component which are present in root directory
	For Each objFile in colFiles
		fileName = objFile.Name
		filePath = objFile.Path
		folderName = ""
		updatedFileName = UpdateNameToComponentStandard(fileName)					'Filename updated as per component standard
		Call AddComponents(fileName, updatedFileName, filePath, folderName, componentFolderName)		
	Next

	'Recursive function for iterating all sub-folders
	ShowSubfolders objFSO.GetFolder(sourceFolder)				'Looping through sub-folders

	'Avoiding memory leakage
	Set objFSO = Nothing
	Set objFolder = Nothing
	Set colFiles = Nothing
	
End Function


'***********************************************************
'Recursice function - Lists files till nth directory and add them as an component
'***********************************************************
Sub ShowSubFolders(Folder)
	
	Dim objFSO, Subfolder, strFolderPath, strPosition, strLength, folderName, objFile
	Dim objFolder :: objFolder = ""
	Dim colFiles, fileName, filePath, updatedFileName, strCurrentFolderName, componentFolderName
	
	Set objFSO = CreateObject("Scripting.FileSystemObject")
	For Each Subfolder in Folder.SubFolders
		
		Set objFolder = objFSO.GetFolder(Subfolder.Path)
		
		'Creating folder components and setting destination path for components
		strCurrentFolderName = objFolder.Name
		strFolderPath = objFolder.Path
		componentFolderName = objFolder.Name

		'Logic for getting the destination path correctly.
		If bitType = "32Bit" Then		
			strPosition = (InStr(Trim(strFolderPath),bitType)) + strBitPosition          
		ElseIf bitType = "64Bit" Then
			strPosition = (InStr(Trim(strFolderPath),bitType)) + strBitPosition			
		ElseIf bitType = "Common" Then
			strPosition = (InStr(Trim(strFolderPath),bitType)) + strBitPosition			
		ElseIf bitType = "Conditional" Then
			strPosition = (InStr(Trim(strFolderPath),bitType)) + strBitPosition			
		End If

		strLength = Len(strFolderPath) - strPosition
		folderName = Right(Trim(strFolderPath), strLength)
		Call AddFolderComponent(folderName, strCurrentFolderName)

		Set colFiles = objFolder.Files		
		For Each objFile in colFiles
			fileName = objFile.Name
			filePath = objFile.Path
			updatedFileName = UpdateNameToComponentStandard(fileName)				'Filename updated as per component standard
			Call AddComponents(fileName, updatedFileName, filePath, folderName, componentFolderName)
		Next	
		ShowSubFolders Subfolder
		folderName = ""
	Next

	'Avoiding memory leakage
	Set objFSO = Nothing
	Set objFolder = Nothing
	Set colFiles = Nothing
	Set objFile = Nothing

End Sub


'***********************************************************
'Function to add components in *.ISM file
'Parameter fileName = Actual file name and component will be deployed with this name.
'Parameter updatedFileName = Special characters in actual file name will be removed. This will be component name.
'Parameter filePath = File path and used to add file for a component.
'***********************************************************
Function AddComponents(fileName, updatedFileName, filePath, folderName, componentFolderName)

	'Variable declaration area.
	Dim componentName :: componentName = updatedFileName
	Dim objComponent, objFeature, objFeatureList, updatedComponentFolderName
	Dim destination :: destination = ""
	
	If folderName <> "" Then 
		destination = "[INSTALLDIR]" + folderName		
	Else
		destination = "[INSTALLDIR]"
	End If

	'Component addition
	updatedComponentFolderName = UpdateNameToComponentStandard(componentFolderName)
	componentName = componentName + "_" + updatedComponentFolderName + "_" + releaseFlag + "_" + bitType
	componentName = componentName & "_" & strFileComponentCount
	'Logging part
	objStdOut.WriteLine "Adding Component " & componentName & " ."

	If Len(componentName) > 60 Then
		Dim extraCharactersLength :: extraCharactersLength = 0
		extraCharactersLength = Len(componentName) - 40
		componentName = Left(componentName, Len(componentName) - extraCharactersLength) & strFileComponentCount
		objStdOut.WriteLine "Updated Component name " & componentName & " ."
	End If

	Set objComponent = objProjectFile.AddComponent( componentName ) ' Creating Component
	objComponent.Addfile( filePath ).DisplayName = fileName			' Adding file for a component	
	objComponent.SharedDLLRefCount = False							' Setting Shared property	
	objComponent.Destination = destination  						' Setting destination property
	objComponent.Condition = updatedComponentCondition 			    ' Setting component condition

	'Adding component to required feature.
	Set objFeature = objProjectFile.ISWiFeatures.Item("Application")
	
	For Each objFeatureList In objFeature.Features
		If objFeatureList.Name = releaseType Then					'Parsing through all available sub-features.
			objFeatureList.AttachComponent objComponent				'Adding component to provided release type
			strFileComponentCount = strFileComponentCount + 1		'Maintaining component count
		End If		
	Next

	'Save and Close ISM file
	objProjectFile.SaveProject	

	'Avoiding memory leakage
	Set objComponent = Nothing	
	Set objFeatureList = Nothing
	Set objFeature = Nothing

End Function

'***********************************************************
'Function to filtering out "",digit and "&" characters from the file name.
'This has to be done since component name does not hold special characters.
'***********************************************************
Function UpdateNameToComponentStandard(Name)

	'Filtering out "" and "&" characters from the file name.
	'At this point we know only 2 spl. characters. When we encounter error with new special character
	'one more line with Replace function has to be added.
	Dim strStandardName :: strStandardName = Name
	strStandardName = Replace(strStandardName, " ", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, "&", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, "-", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, "{", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, "}", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, "(", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, ")", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, ",", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, "[", "_", 1, -1, 1)
	strStandardName = Replace(strStandardName, "]", "_", 1, -1, 1)
	
	'If first character is numeric adding "_"
	If (IsNumeric(Left(strStandardName, 1))) Then
		strStandardName = "_" & strStandardName
	End If
	
	'If first character is "." adding "_"
	If (Left(strStandardName, 1) = ".") Then
		strStandardName = "_" & strStandardName
	End If
	
	UpdateNameToComponentStandard = strStandardName

End Function

'***********************************************************
'Function to add required folder structure
'***********************************************************
Function AddFolderComponent(folderName, strCurrentFolderName)

	'Variable declaration area.
	Dim componentName :: componentName = strCurrentFolderName
	Dim componentExists :: componentExists = "False"
	Dim objComponent, objFeatureList, objFeature, compCheck
	Dim destination :: destination = "[INSTALLDIR]" + folderName

	'Updating component name, as per installer standard.
	componentName = UpdateNameToComponentStandard(componentName)
	componentName = componentName + "FolderComponent"

	'Component addition
	For Each compCheck In objProjectFile.ISWiComponents
		If StrComp(Trim(compCheck.Name),Trim(componentName)) = 0 Then 
			componentExists = "True"
			Set objComponent = compCheck
		End If
	Next
	
	If componentExists = "False" Then			'Creating Component only if it does not exist

		If Len(componentName) > 60 Then
		Dim extraCharactersLength :: extraCharactersLength = 0
			extraCharactersLength = Len(componentName) - 40
			componentName = Left(componentName, Len(componentName) - extraCharactersLength) & strFolderComponentCount
			objStdOut.WriteLine "Updated Component name " & componentName & " ."
		End If

		Set objComponent = objProjectFile.AddComponent(componentName) 
		objComponent.Destination = destination
		objComponent.SharedDLLRefCount = False							' Setting Shared property
	End If

	'Attaching component to required feature.
	Set objFeature = objProjectFile.ISWiFeatures.Item("Application")	
	For Each objFeatureList In objFeature.Features
		If objFeatureList.Name = releaseType Then   				'Parsing through all available sub-features.
			objFeatureList.AttachComponent objComponent             'Adding component to provided release type
			strFolderComponentCount = strFolderComponentCount + 1	'Maintaining component count
		End If		
	Next	

	'Save and Close ISM file
	objProjectFile.SaveProject	

	'Avoiding memory leakage
	Set objComponent = Nothing
	Set objFeatureList = Nothing
	Set objFeature = Nothing
	Set compCheck = Nothing	

End Function

'****************************************************************
'Function is used to calculate difference between 2 time entry. 
'There is no direct function available for this task in VBScript.
'****************************************************************
Function CallTimeSeconds(StartTime,EndTime)
	
	'variable declaration part
	Dim StartHour, StartMin, StartSec, EndHour, EndMin, EndSec
	Dim StartingSeconds, EndingSeconds

	StartHour = Hour(StartTime)
	StartMin =  Minute(StartTime)
	StartSec = Second(StartTime)
	EndHour = Hour(EndTime)
	EndMin = Minute(EndTime)
	EndSec = Second(EndTime)
	StartingSeconds = (StartSec + (StartMin * 60) + ((StartHour * 60)*60))
	EndingSeconds = (EndSec + (EndMin * 60) + ((EndHour * 60)*60))
	CallTimeSeconds = EndingSeconds - StartingSeconds

End Function