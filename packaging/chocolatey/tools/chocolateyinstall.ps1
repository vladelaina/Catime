$ErrorActionPreference = 'Stop'

$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$exePath = Join-Path $toolsDir 'catime.exe'
$url = 'https://github.com/vladelaina/Catime/releases/download/v1.5.0/catime_1.5.0.exe'
$checksum = 'f89c30c2d076d22870eceee79672d9ed0f5742f994c624a9bb1a7dd61bea1e8d'

$runningProcesses = Get-Process -Name 'catime' -ErrorAction SilentlyContinue
if ($runningProcesses) {
    $runningProcesses | Stop-Process -Force
}

Get-ChocolateyWebFile `
    -PackageName $env:ChocolateyPackageName `
    -FileFullPath $exePath `
    -Url $url `
    -Checksum $checksum `
    -ChecksumType 'sha256'

$programs = [Environment]::GetFolderPath('Programs')
$shortcutPath = Join-Path $programs 'Catime.lnk'
Install-ChocolateyShortcut -ShortcutFilePath $shortcutPath -TargetPath $exePath
