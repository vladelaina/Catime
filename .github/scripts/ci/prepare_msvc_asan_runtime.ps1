param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryPath
)

$ErrorActionPreference = "Stop"

function Find-VsWhere {
    $candidate = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $candidate) {
        return $candidate
    }
    throw "vswhere.exe was not found"
}

function Find-AsanDlls {
    $vswhere = Find-VsWhere
    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installPath) {
        throw "Visual Studio installation path was not found"
    }

    $patterns = @(
        "clang_rt.asan*_dynamic-i386.dll",
        "clang_rt.asan*_dynamic_runtime_thunk-i386.dll"
    )

    $dlls = @()
    foreach ($pattern in $patterns) {
        $found = Get-ChildItem -Path (Join-Path $installPath "VC\Tools\MSVC") -Filter $pattern -Recurse -File -ErrorAction SilentlyContinue
        if ($found) {
            $dlls += $found
        }
    }

    $unique = @($dlls | Sort-Object FullName -Unique)
    if ($unique.Count -eq 0) {
        throw "MSVC ASan runtime DLLs for Win32 were not found"
    }
    return $unique
}

$binaryDir = Split-Path $BinaryPath -Parent
$dlls = Find-AsanDlls

foreach ($dll in $dlls) {
    Copy-Item -Path $dll.FullName -Destination (Join-Path $binaryDir $dll.Name) -Force
    Write-Output "Prepared ASan runtime: $($dll.Name)"
}
