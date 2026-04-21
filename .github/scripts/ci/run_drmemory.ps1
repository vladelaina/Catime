param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string[]]$Arguments = @("--ci-smoke", "--ci-exit-ms=4000")
)

$ErrorActionPreference = "Stop"

function Find-DrMemory {
    $cmd = Get-Command drmemory.exe -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    $roots = @(
        "$env:ChocolateyInstall\lib",
        "${env:ProgramFiles(x86)}\Dr. Memory",
        "$env:ProgramFiles\Dr. Memory"
    )

    foreach ($root in $roots) {
        if (-not (Test-Path $root)) {
            continue
        }
        $candidate = Get-ChildItem -Path $root -Filter drmemory.exe -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if ($candidate) {
            return $candidate
        }
    }

    throw "drmemory.exe was not found"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$drmemory = Find-DrMemory

$commandArgs = @(
    "-batch",
    "-brief",
    "-logdir", $OutputDir,
    "-exit_code_if_errors", "9",
    "--", $BinaryPath
) + $Arguments

$stdoutPath = Join-Path $OutputDir "drmemory-stdout.txt"
$stderrPath = Join-Path $OutputDir "drmemory-stderr.txt"

$process = Start-Process -FilePath $drmemory -ArgumentList $commandArgs -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

$detailLines = @()
$errorCount = 0

Get-ChildItem -Path $OutputDir -Filter "results*.txt" -File -ErrorAction SilentlyContinue | ForEach-Object {
    $content = Get-Content $_.FullName
    foreach ($line in $content) {
        if ($line -match "^Error #") {
            $errorCount++
            if ($detailLines.Count -lt 5) {
                $detailLines += $line.Trim()
            }
        }
    }
}

$status = if ($process.ExitCode -eq 0 -and $errorCount -eq 0) { "passed" } else { "failed" }
$summary = if ($status -eq "passed") {
    "Dr. Memory completed without reported issues"
} else {
    "Dr. Memory reported $errorCount issue(s)"
}

if ($status -ne "passed") {
    foreach ($line in $detailLines) {
        Write-Output "::error::$line"
    }
    if ($detailLines.Count -eq 0) {
        Write-Output "::error::$summary"
    }
    exit 1
}

Write-Output "::notice::$summary"
