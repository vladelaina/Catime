param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir,

    [string[]]$Arguments = @("--ci-smoke", "--ci-exit-ms=4000")
)

$ErrorActionPreference = "Stop"

function Find-Tool {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ExeName
    )

    $command = Get-Command $ExeName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $roots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64",
        "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x86",
        "${env:ProgramFiles(x86)}\Windows Kits\10\App Certification Kit",
        "${env:ProgramFiles}\Windows Kits\10\Debuggers\x64",
        "${env:SystemRoot}\System32"
    )

    foreach ($root in $roots) {
        if (-not (Test-Path $root)) {
            continue
        }
        $candidate = Get-ChildItem -Path $root -Filter $ExeName -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1 -ExpandProperty FullName
        if ($candidate) {
            return $candidate
        }
    }

    throw "$ExeName was not found"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$appverif = Find-Tool -ExeName "appverif.exe"
$gflags = Find-Tool -ExeName "gflags.exe"
$binaryName = Split-Path $BinaryPath -Leaf
$binaryDir = Split-Path $BinaryPath -Parent
$stdoutPath = Join-Path $OutputDir "appverif-stdout.txt"
$stderrPath = Join-Path $OutputDir "appverif-stderr.txt"
$queryPath = Join-Path $OutputDir "appverif-query.txt"
$pageHeapPath = Join-Path $OutputDir "gflags-pageheap.txt"
$xmlPath = Join-Path $OutputDir "appverif.xml"
$env:VERIFIER_LOG_PATH = (Resolve-Path $OutputDir).Path

try {
    & $appverif -delete logs -for $binaryName *> $null
    & $appverif -delete settings -for $binaryName *> $null
    & $gflags /p /disable $binaryName *> $null

    & $appverif /verify $binaryName
    & $appverif -query * -for $binaryName | Out-File -Encoding utf8 $queryPath
    & $gflags /p /enable $binaryName /full /leaks
    & $gflags /p | Out-File -Encoding utf8 $pageHeapPath

    $process = Start-Process -FilePath $BinaryPath -ArgumentList $Arguments -WorkingDirectory $binaryDir -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath

    $detailLines = @()
    $findingCount = 0

    & $appverif -export log -for $binaryName -with "To=$xmlPath" *> $null

    if (Test-Path $xmlPath) {
        try {
            [xml]$xml = Get-Content $xmlPath
            $entries = @(Select-Xml -Xml $xml -XPath "//*[local-name()='logEntry']")
            $findingCount = $entries.Count
            foreach ($entry in $entries | Select-Object -First 5) {
                $text = ($entry.Node.InnerText -replace "\s+", " ").Trim()
                if ($text) {
                    $detailLines += $text
                }
            }
        } catch {
            $content = Get-Content $xmlPath -Raw
            $findingCount = ([regex]::Matches($content, "<([^:>]+:)?logEntry\b")).Count
            if ($findingCount -gt 0) {
                $detailLines += "Application Verifier exported log entries to appverif.xml"
            }
        }
    }

    if ($process.ExitCode -ne 0 -and $detailLines.Count -lt 5) {
        $detailLines += "Smoke run exited with code $($process.ExitCode)"
    }

    $reportedFindings = if ($process.ExitCode -eq 0) { $findingCount } else { [Math]::Max($findingCount, 1) }
    $status = if ($process.ExitCode -eq 0 -and $findingCount -eq 0) { "passed" } else { "failed" }
    $summary = if ($status -eq "passed") {
        "Application Verifier Basics + PageHeap completed without reported issues"
    } else {
        "Application Verifier reported $reportedFindings issue marker(s)"
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
} catch {
    Write-Output "::error::$($_.Exception.Message)"
    exit 1
} finally {
    & $gflags /p /disable $binaryName *> $null
    & $appverif -delete settings -for $binaryName *> $null
}
