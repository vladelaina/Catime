param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"

function Find-Dumpbin {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installPath) {
            $candidate = Get-ChildItem -Path (Join-Path $installPath "VC\Tools\MSVC") -Filter dumpbin.exe -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 1 -ExpandProperty FullName
            if ($candidate) {
                return $candidate
            }
        }
    }

    $fallback = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($fallback) {
        return $fallback.Source
    }

    throw "dumpbin.exe was not found"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$dumpbin = Find-Dumpbin
$headersPath = Join-Path $OutputDir "dumpbin-headers.txt"
$loadConfigPath = Join-Path $OutputDir "dumpbin-loadconfig.txt"

& $dumpbin /headers $BinaryPath | Out-File -Encoding utf8 $headersPath
& $dumpbin /loadconfig $BinaryPath | Out-File -Encoding utf8 $loadConfigPath

$headers = Get-Content $headersPath -Raw
$loadConfig = Get-Content $loadConfigPath -Raw

$checks = @(
    @{ Name = "ASLR / Dynamic base"; Passed = ($headers -match "Dynamic base") },
    @{ Name = "DEP / NX compatible"; Passed = ($headers -match "NX compatible") },
    @{ Name = "Control Flow Guard"; Passed = ($loadConfig -match "CF Instrumented") }
)

$missing = @($checks | Where-Object { -not $_.Passed } | ForEach-Object { $_.Name })
$status = if ($missing.Count -eq 0) { "passed" } else { "failed" }
$summary = if ($missing.Count -eq 0) {
    "Win32 PE hardening flags are present"
} else {
    "Missing hardening flags: $($missing -join ', ')"
}

if ($missing.Count -gt 0) {
    foreach ($item in $missing) {
        Write-Output "::error::$item"
    }
    Write-Error $summary
} else {
    Write-Output $summary
}
