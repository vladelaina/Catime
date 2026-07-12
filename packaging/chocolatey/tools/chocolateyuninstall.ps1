$ErrorActionPreference = 'Stop'

$runningProcesses = Get-Process -Name 'catime' -ErrorAction SilentlyContinue
if ($runningProcesses) {
    $runningProcesses | Stop-Process -Force
}

$programs = [Environment]::GetFolderPath('Programs')
$shortcutPath = Join-Path $programs 'Catime.lnk'
if (Test-Path $shortcutPath) {
    Remove-Item $shortcutPath -Force
}

# User configuration under %LOCALAPPDATA%\Catime is intentionally preserved.
