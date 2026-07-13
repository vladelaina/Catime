param(
    [Parameter(Mandatory = $true)]
    [string]$ArchiveDirectory,

    [Parameter(Mandatory = $true)]
    [string]$InstallDirectory,

    [Parameter(Mandatory = $true)]
    [string]$ArchiveName,

    [Parameter(Mandatory = $true)]
    [string]$DownloadUrl,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedSha256
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Path $ArchiveDirectory -Force | Out-Null
$archivePath = Join-Path $ArchiveDirectory $ArchiveName

if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
    Write-Host "Downloading pinned WinLibs i686 toolchain..."
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $archivePath
}

$actualSha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
if ($actualSha256 -ne $ExpectedSha256) {
    throw "WinLibs archive SHA-256 mismatch: expected $ExpectedSha256, got $actualSha256"
}

$binPath = Join-Path $InstallDirectory "mingw32\bin"
if (-not (Test-Path -LiteralPath (Join-Path $binPath "gcc.exe") -PathType Leaf)) {
    New-Item -ItemType Directory -Path $InstallDirectory -Force | Out-Null
    Write-Host "Extracting pinned WinLibs i686 toolchain..."
    Expand-Archive -LiteralPath $archivePath -DestinationPath $InstallDirectory -Force
}

$gccPath = Join-Path $binPath "gcc.exe"
$target = (& $gccPath -dumpmachine).Trim()
$version = (& $gccPath -dumpfullversion).Trim()
if ($LASTEXITCODE -ne 0 -or $target -ne "i686-w64-mingw32" -or $version -ne "16.1.0") {
    throw "Unexpected WinLibs compiler: target=$target version=$version"
}

Write-Host "Using WinLibs GCC $version for $target"
Write-Host "Verified archive SHA-256: $actualSha256"

if ($env:GITHUB_PATH) {
    $binPath | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append
}
if ($env:GITHUB_ENV) {
    "WINLIBS_BIN=$binPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}
