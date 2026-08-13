[CmdletBinding()]
param(
    [string]$Server = '',
    [int]$Port = 6001,
    [string]$User = 'root',
    [string]$Password = '111',
    [switch]$RequireConnection
)

$ErrorActionPreference = 'Stop'
$driverNames = @(
    'MatrixOne ODBC 9.7 Unicode Driver',
    'MatrixOne ODBC 9.7 ANSI Driver'
)

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

if (-not ('NativeLibrary' -as [type])) {
    Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class NativeLibrary {
    [DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)]
    private static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);

    [DllImport("kernel32", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr module);

    public static void LoadAndFree(string path) {
        const uint LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR = 0x00000100;
        const uint LOAD_LIBRARY_SEARCH_DEFAULT_DIRS = 0x00001000;
        IntPtr module = LoadLibraryExW(path, IntPtr.Zero,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (module == IntPtr.Zero) {
            throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot load " + path);
        }
        FreeLibrary(module);
    }
}
'@
}

$drivers = @(Get-OdbcDriver -Platform '64-bit' |
    Where-Object Name -In $driverNames)
Assert-True ($drivers.Count -eq 2) "Expected two MatrixOne x64 drivers, found $($drivers.Count)."

foreach ($name in $driverNames) {
    $key = "HKLM:\SOFTWARE\ODBC\ODBCINST.INI\$name"
    $entry = Get-ItemProperty -LiteralPath $key
    foreach ($property in 'Driver', 'Setup') {
        $path = $entry.$property
        Assert-True ([System.IO.Path]::IsPathRooted($path)) "$name $property is not an absolute path: $path"
        Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "$name $property is missing: $path"
        [NativeLibrary]::LoadAndFree($path)
    }
}

if ($RequireConnection) {
    Assert-True (-not [string]::IsNullOrWhiteSpace($Server)) 'Server is required for a connection test.'
    foreach ($name in $driverNames) {
        $connectionString = "DRIVER={$name};SERVER=$Server;PORT=$Port;UID=$User;PWD=$Password;SSLMODE=DISABLED"
        $connection = [System.Data.Odbc.OdbcConnection]::new($connectionString)
        try {
            $connection.Open()
            $command = $connection.CreateCommand()
            try {
                $command.CommandText = "SELECT '安装测试', 123.45"
                $reader = $command.ExecuteReader()
                try {
                    Assert-True ($reader.Read()) "$name returned no row."
                    Assert-True ($reader.GetString(0) -eq '安装测试') "$name Unicode round trip failed."
                    Assert-True ([decimal]$reader.GetValue(1) -eq [decimal]123.45) "$name decimal round trip failed."
                } finally { $reader.Dispose() }
            } finally { $command.Dispose() }
        } finally { $connection.Dispose() }
    }
}

[pscustomobject]@{
    Drivers = $driverNames
    Connected = [bool]$RequireConnection
    Server = $Server
} | ConvertTo-Json -Compress
