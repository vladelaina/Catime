param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string]$Version = "4.4.8"
)

$ErrorActionPreference = "Stop"

function Find-BinSkim {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $candidate = Get-ChildItem -Path $Root -Filter BinSkim.exe -Recurse -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $candidate) {
        throw "BinSkim.exe was not found in $Root"
    }
    return $candidate
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$packageRoot = Join-Path $OutputDir "binskim-package"
$packagePath = Join-Path $OutputDir "Microsoft.CodeAnalysis.BinSkim.$Version.nupkg"
$downloadUri = "https://www.nuget.org/api/v2/package/Microsoft.CodeAnalysis.BinSkim/$Version"

Invoke-WebRequest -Uri $downloadUri -OutFile $packagePath

if (Test-Path $packageRoot) {
    Remove-Item -Recurse -Force $packageRoot
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::ExtractToDirectory($packagePath, $packageRoot)

$binskim = Find-BinSkim -Root $packageRoot
$sarifPath = Join-Path $OutputDir "binskim.sarif"
$stdoutPath = Join-Path $OutputDir "binskim-stdout.txt"
$stderrPath = Join-Path $OutputDir "binskim-stderr.txt"

$arguments = @(
    "analyze",
    $BinaryPath,
    "--recurse", "false",
    "--quiet", "true",
    "--output", $sarifPath,
    "--rich-return-code", "true"
)

$process = Start-Process -FilePath $binskim -ArgumentList $arguments -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

exit $process.ExitCode
