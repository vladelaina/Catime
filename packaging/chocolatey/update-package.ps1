[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$Checksum
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$packageDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$nuspecPath = Join-Path $packageDirectory 'catime.nuspec'
$installPath = Join-Path $packageDirectory 'tools\chocolateyinstall.ps1'
$verificationPath = Join-Path $packageDirectory 'tools\VERIFICATION.txt'
$releaseUrl = "https://github.com/vladelaina/Catime/releases/download/v$Version/catime_$Version.exe"
$normalizedChecksum = $Checksum.ToLowerInvariant()

function Set-FileContentUtf8 {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    [System.IO.File]::WriteAllText(
        $Path,
        $Content,
        [System.Text.UTF8Encoding]::new($false))
}

function Replace-SingleMatch {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Replacement,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $matches = [regex]::Matches($Text, $Pattern)
    if ($matches.Count -ne 1) {
        throw "Expected exactly one $Label entry, found $($matches.Count)."
    }
    return [regex]::Replace($Text, $Pattern, $Replacement, 1)
}

$nuspec = Get-Content $nuspecPath -Raw
$nuspec = Replace-SingleMatch `
    -Text $nuspec `
    -Pattern '<version>[^<]+</version>' `
    -Replacement "<version>$Version</version>" `
    -Label 'nuspec version'
$nuspec = Replace-SingleMatch `
    -Text $nuspec `
    -Pattern '<releaseNotes>[^<]+</releaseNotes>' `
    -Replacement "<releaseNotes>https://github.com/vladelaina/Catime/releases/tag/v$Version</releaseNotes>" `
    -Label 'release notes URL'
Set-FileContentUtf8 -Path $nuspecPath -Content $nuspec

$install = Get-Content $installPath -Raw
$install = Replace-SingleMatch `
    -Text $install `
    -Pattern '(?m)^\$url = ''[^'']+''$' `
    -Replacement "`$url = '$releaseUrl'" `
    -Label 'installer URL'
$install = Replace-SingleMatch `
    -Text $install `
    -Pattern '(?m)^\$checksum = ''[0-9a-fA-F]{64}''$' `
    -Replacement "`$checksum = '$normalizedChecksum'" `
    -Label 'installer checksum'
Set-FileContentUtf8 -Path $installPath -Content $install

$verification = @"
VERIFICATION

Verification is intended to assist the Chocolatey moderators and community in
verifying that this package's contents are trustworthy.

The installer downloads the official Catime release executable from:

$releaseUrl

The expected SHA256 checksum is:

$normalizedChecksum

To verify manually, download the file and run:

Get-FileHash .\catime_$Version.exe -Algorithm SHA256
"@
Set-FileContentUtf8 -Path $verificationPath -Content $verification

Write-Host "Prepared Chocolatey package for Catime $Version"
Write-Host "Release URL: $releaseUrl"
Write-Host "SHA256: $normalizedChecksum"
