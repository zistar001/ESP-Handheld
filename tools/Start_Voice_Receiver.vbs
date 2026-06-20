Dim fso, dir, pyw
Set fso = CreateObject("Scripting.FileSystemObject")
dir = fso.GetParentFolderName(WScript.ScriptFullName)

' 使用 venv 内的 pythonw（依赖已安装）
pyw = dir & "\.venv\Scripts\pythonw.exe"
If Not fso.FileExists(pyw) Then
    ' 回退到 uv 系统 pythonw
    pyw = "C:\Users\Administrator\AppData\Roaming\uv\python\cpython-3.14.3-windows-x86_64-none\pythonw.exe"
End If

CreateObject("WScript.Shell").Run """" & pyw & """ """ & dir & "\run_voice_receiver.pyw""", 0, False
