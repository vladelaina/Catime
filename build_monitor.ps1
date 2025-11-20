# PowerShell build monitor with real-time progress
param(
    [string]$BuildDir = ".",
    [int]$TotalFiles = 95
)

$e = [char]27

function Show-Progress {
    param(
        [int]$Current,
        [int]$Total,
        [string]$Message
    )
    
    $percentage = [math]::Floor(($Current * 100) / $Total)
    $filled = [math]::Floor(($Current * 40) / $Total)
    $empty = 40 - $filled
    
    # Choose message color based on stage
    $messageColor = "38;2;255;100;150" # Pink/red for compiling
    if ($Message -match "Configuring") {
        $messageColor = "38;2;100;200;255" # Light blue
    } elseif ($Message -match "Analyzing") {
        $messageColor = "38;2;255;200;100" # Orange
    } elseif ($Message -match "Finalizing") {
        $messageColor = "38;2;100;255;150" # Green
    }
    
    # Build progress bar
    $progress = "#" * $filled
    $dots = "." * $empty
    
    # Write progress with carriage return (same line update)
    Write-Host -NoNewline "`r${e}[1m${e}[38;2;147;112;219m[$($percentage.ToString().PadLeft(3))%]${e}[0m "
    Write-Host -NoNewline "${e}[38;2;138;43;226m$progress${e}[38;2;80;80;80m$dots${e}[0m "
    Write-Host -NoNewline "${e}[$messageColor`m$Message${e}[0m"
}

# Monitor build progress
$currentFiles = 0
$lastCheckTime = Get-Date

while ($true) {
    # Check if build is complete
    if (Test-Path "build_complete.tmp") {
        break
    }
    
    # Count compiled object files
    $compiled = @(Get-ChildItem -Path $BuildDir -Filter "*.obj" -Recurse -ErrorAction SilentlyContinue).Count
    
    # Update progress if changed
    if ($compiled -ne $currentFiles) {
        $currentFiles = $compiled
        $lastCheckTime = Get-Date
        
        if ($currentFiles -le $TotalFiles) {
            Show-Progress -Current $currentFiles -Total $TotalFiles -Message "Compiling ($currentFiles/$TotalFiles files)..."
        }
    }
    
    # Check for timeout (no progress for 30 seconds)
    $timeDiff = ((Get-Date) - $lastCheckTime).TotalSeconds
    if ($timeDiff -gt 30) {
        # Check if build process might be stuck
        if (-not (Test-Path "build_complete.tmp")) {
            # Still waiting, but check for very long timeout
            if ($timeDiff -gt 600) {
                Write-Host "`n${e}[93m⚠ Build timeout (no progress for 10 minutes)${e}[0m"
                break
            }
        }
    }
    
    Start-Sleep -Milliseconds 200
}

# Final progress update
if (Test-Path "$BuildDir\catime.exe") {
    Show-Progress -Current $TotalFiles -Total $TotalFiles -Message "Compiling source files... done"
    Write-Host ""
}
