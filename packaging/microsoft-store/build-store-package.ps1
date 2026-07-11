[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$repositoryRoot = (Resolve-Path (Join-Path $scriptDirectory "..\..")).Path

if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repositoryRoot "output\microsoft-store"
}

$ExecutablePath = (Resolve-Path $ExecutablePath).Path
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

if ($Version -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
    throw "Version must use Major.Minor.Patch format, for example 1.5.0."
}

$versionParts = @(
    [int]$Matches[1],
    [int]$Matches[2],
    [int]$Matches[3]
)
if ($versionParts[0] -eq 0 -or ($versionParts | Where-Object { $_ -gt 65535 })) {
    throw "MSIX version components must be between 0 and 65535 and Major cannot be zero."
}

$packageVersion = "$($versionParts[0]).$($versionParts[1]).$($versionParts[2]).0"
$packageBaseName = "catime_${Version}_x86"
$msixPath = Join-Path $OutputDirectory "$packageBaseName.msix"
$uploadPath = Join-Path $OutputDirectory "$packageBaseName.msixupload"
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("CatimeStorePackage_" + [guid]::NewGuid())
$stagingDirectory = Join-Path $temporaryRoot "package"
$assetsDirectory = Join-Path $stagingDirectory "Assets"
$uploadDirectory = Join-Path $temporaryRoot "upload"
$certificatePath = Join-Path $temporaryRoot "CatimeStoreTemporary.pfx"
$certificate = $null

function Get-WindowsSdkTool {
    param([Parameter(Mandatory = $true)][string]$Name)

    $sdkRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (Test-Path $sdkRoot) {
        $sdkVersions = Get-ChildItem $sdkRoot -Directory |
            Where-Object { $_.Name -match '^\d+\.\d+' } |
            Sort-Object { [version]$_.Name } -Descending

        foreach ($sdkVersion in $sdkVersions) {
            $candidate = Join-Path $sdkVersion.FullName "x64\$Name"
            if (Test-Path $candidate) {
                return $candidate
            }
        }
    }

    $appCertKitCandidate = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\App Certification Kit\$Name"
    if (Test-Path $appCertKitCandidate) {
        return $appCertKitCandidate
    }

    throw "$Name was not found. Install the Windows 10/11 SDK."
}

function New-StoreLogo {
    param(
        [Parameter(Mandatory = $true)][object]$Source,
        [Parameter(Mandatory = $true)][int]$Width,
        [Parameter(Mandatory = $true)][int]$Height,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    $bitmap = [System.Drawing.Bitmap]::new(
        $Width,
        $Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

            $scale = [Math]::Min($Width / $Source.Width, $Height / $Source.Height)
            $drawWidth = [Math]::Max(1, [int][Math]::Round($Source.Width * $scale))
            $drawHeight = [Math]::Max(1, [int][Math]::Round($Source.Height * $scale))
            $drawX = [int](($Width - $drawWidth) / 2)
            $drawY = [int](($Height - $drawHeight) / 2)
            $graphics.DrawImage($Source, $drawX, $drawY, $drawWidth, $drawHeight)
        }
        finally {
            $graphics.Dispose()
        }

        $bitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

try {
    New-Item -ItemType Directory -Force -Path $OutputDirectory, $stagingDirectory, $assetsDirectory, $uploadDirectory | Out-Null
    Remove-Item $msixPath, $uploadPath -Force -ErrorAction SilentlyContinue

    Copy-Item $ExecutablePath (Join-Path $stagingDirectory "catime.exe")

    $manifestTemplate = Get-Content (Join-Path $scriptDirectory "AppxManifest.xml.in") -Raw
    $manifest = $manifestTemplate.Replace("@PACKAGE_VERSION@", $packageVersion)
    [System.IO.File]::WriteAllText(
        (Join-Path $stagingDirectory "AppxManifest.xml"),
        $manifest,
        [System.Text.UTF8Encoding]::new($false))

    Add-Type -AssemblyName System.Drawing
    $sourceLogo = [System.Drawing.Image]::FromFile((Join-Path $repositoryRoot "Images\catime.png"))
    try {
        New-StoreLogo $sourceLogo 50 50 (Join-Path $assetsDirectory "StoreLogo.png")
        New-StoreLogo $sourceLogo 44 44 (Join-Path $assetsDirectory "Square44x44Logo.png")
        New-StoreLogo $sourceLogo 150 150 (Join-Path $assetsDirectory "Square150x150Logo.png")
    }
    finally {
        $sourceLogo.Dispose()
    }

    $makeAppx = Get-WindowsSdkTool "makeappx.exe"
    $signTool = Get-WindowsSdkTool "signtool.exe"

    & $makeAppx pack /o /h SHA256 /d $stagingDirectory /p $msixPath
    if ($LASTEXITCODE -ne 0) {
        throw "MakeAppx failed with exit code $LASTEXITCODE."
    }

    $publisher = "CN=5503A135-7FA4-466B-815C-DBE627F4065F"
    $certificate = New-SelfSignedCertificate `
        -Type Custom `
        -KeyUsage DigitalSignature `
        -KeyExportPolicy Exportable `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}") `
        -Subject $publisher `
        -FriendlyName "Catime temporary Microsoft Store package signing"

    $plainPassword = [guid]::NewGuid().ToString("N")
    $securePassword = ConvertTo-SecureString $plainPassword -AsPlainText -Force
    Export-PfxCertificate -Cert $certificate -FilePath $certificatePath -Password $securePassword | Out-Null

    & $signTool sign /fd SHA256 /f $certificatePath /p $plainPassword $msixPath
    if ($LASTEXITCODE -ne 0) {
        throw "SignTool failed with exit code $LASTEXITCODE."
    }

    Copy-Item $msixPath (Join-Path $uploadDirectory (Split-Path $msixPath -Leaf))
    $zipPath = [System.IO.Path]::ChangeExtension($uploadPath, ".zip")
    Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $uploadDirectory "*") -DestinationPath $zipPath -CompressionLevel Optimal
    Move-Item $zipPath $uploadPath

    Write-Host "Microsoft Store package created: $msixPath"
    Write-Host "Partner Center upload created: $uploadPath"
    Write-Host "Package identity version: $packageVersion"
}
finally {
    if ($certificate) {
        Remove-Item ("Cert:\CurrentUser\My\" + $certificate.Thumbprint) -Force -ErrorAction SilentlyContinue
    }
    Remove-Item $temporaryRoot -Recurse -Force -ErrorAction SilentlyContinue
}
