' Launches DownloadsJanitor.exe without opening a console window.
Dim shell, fso, scriptDir, projectDir, exeDir, exePath, configDir

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)
projectDir = fso.GetParentFolderName(scriptDir)
exeDir = projectDir & "\build-win"
exePath = exeDir & "\DownloadsJanitor.exe"
configDir = exeDir & "\config"

If Not fso.FileExists(exePath) Then
    MsgBox "DownloadsJanitor.exe was not found at:" & vbCrLf & exePath, vbCritical, "DownloadsJanitor"
    WScript.Quit 1
End If

If Not fso.FolderExists(configDir) Then
    MsgBox "The config folder was not found next to the executable:" & vbCrLf & configDir, vbExclamation, "DownloadsJanitor"
    WScript.Quit 1
End If

shell.CurrentDirectory = exeDir
shell.Run """" & exePath & """", 0, False
