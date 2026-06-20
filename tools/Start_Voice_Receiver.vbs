Dim fso, dir, pyw
Set fso = CreateObject("Scripting.FileSystemObject")
dir = fso.GetParentFolderName(WScript.ScriptFullName) ' 脚本所在目录

' uv管理的pythonw
pyw = "C:\Users\Administrator\AppData\Roaming\uv\python\cpython-3.14.3-windows-x86_64-none\pythonw.exe"
If Not fso.FileExists(pyw) Then
    pyw = "C:\Users\Administrator\AppData\Roaming\uv\python\cpython-3.14-windows-x86_64-none\pythonw.exe"
End If

CreateObject("WScript.Shell").Run """" & pyw & """ """ & dir & "\run_voice_receiver.pyw""", 0, False
