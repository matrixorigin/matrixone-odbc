If CreateObject("Scripting.FileSystemObject").FolderExists(Session.Property("PBIEXINSTALLDIR")) Then
  Session.Property("PBI_WARNING") = "0"
Else
  Session.Property("PBI_WARNING") = "1"
End if
