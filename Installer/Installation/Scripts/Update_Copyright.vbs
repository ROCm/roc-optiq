'****************************************************************************************
'This script updates copyright informtaion within installer project.
'The script searches for ism file within the folder where it is kept.
'Once found it updates all release types of each ism file with copyright year as current year when build happens.
'Also provides logging during build on which all ism was identified and updated.
'****************************************************************************************

Option Explicit

Dim objFSO, objRootFolder, colFiles, objFile, objProjectFile, pProductConfigs, pReleases, objStdOut
Set objFSO = CreateObject("Scripting.FileSystemObject")
Set objStdOut = WScript.StdOut

'Traverse through all files in current dirctory to find ism files

Dim rootSourceFolder : rootSourceFolder = objFSO.GetParentFolderName(WScript.ScriptFullName)
Set objRootFolder = objFSO.GetFolder(rootSourceFolder)
	Set colFiles = objRootFolder.Files
		For Each objFile in colFiles
			If (objFSO.GetExtensionName(objFile.name)) = "ism" Then 'Get ism files in the folder
				
				'Setting up ISM project 
				Dim ismFile : ismFile = objFile.path
				objStdOut.WriteLine	"Current year obtained for copyright year updation : " & WScript.Arguments(0)
				objStdOut.WriteLine "File selected for copyright year updation : " &  ismFile
				Set objProjectFile = CreateObject ("IswiAuto29.ISWiProject")
				objProjectFile.OpenProject ismFile

				'Iterates through all product configuration and releases within ism and updates copyright in each of them
				For Each pProductConfigs In objProjectFile.ISWiProductConfigs
					For Each pReleases In pProductConfigs.ISWiReleases
						objStdOut.WriteLine	"Release type updated : " & pReleases.Name
						pReleases.UseMyVersionInfo  = True
						pReleases.LauncherCopyright = "(C) " & WScript.Arguments(0) & " Advanced Micro Devices, Inc."
					Next
				Next
				
				'Save and Close ISM file
				objProjectFile.SaveProject
				
				objStdOut.WriteLine	""
				
			End If
		Next

'Avoiding memory leakage
Set objRootFolder = Nothing	
Set objProjectFile = Nothing
Set objFile = Nothing
Set colFiles = Nothing
Set pProductConfigs = Nothing
Set pReleases = Nothing
