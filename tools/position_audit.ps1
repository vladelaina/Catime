param(
    [string]$ExePath = ".\build\catime.exe",
    [int]$Iterations = 3
)

$ErrorActionPreference = "Stop"

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class CatimePositionAuditNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int x, int y);

    [DllImport("user32.dll")]
    public static extern bool GetCursorPos(out POINT point);

    [DllImport("user32.dll")]
    public static extern IntPtr SendMessage(
        IntPtr hwnd, uint message, UIntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool SetWindowPos(
        IntPtr hwnd, IntPtr insertAfter,
        int x, int y, int width, int height, uint flags);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);

    [DllImport("user32.dll")]
    public static extern int GetSystemMetrics(int index);
}
'@

function New-UIntPtr([uint64]$Value) {
    return [UIntPtr]::new($Value)
}

function Get-AuditRect([IntPtr]$Hwnd) {
    $rect = New-Object CatimePositionAuditNative+RECT
    if (-not [CatimePositionAuditNative]::GetWindowRect($Hwnd, [ref]$rect)) {
        throw "GetWindowRect failed"
    }
    return $rect
}

function Set-AuditCursor([int]$X, [int]$Y) {
    if (-not [CatimePositionAuditNative]::SetCursorPos($X, $Y)) {
        throw "SetCursorPos failed"
    }
    Start-Sleep -Milliseconds 35
}

function Enter-AuditEditMode([IntPtr]$Hwnd) {
    [CatimePositionAuditNative]::SendMessage(
        $Hwnd, 0x0111, (New-UIntPtr 113), [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 250
}

function Exit-AuditEditMode([IntPtr]$Hwnd) {
    [CatimePositionAuditNative]::SendMessage(
        $Hwnd, 0x0204, [UIntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 30
    [CatimePositionAuditNative]::SendMessage(
        $Hwnd, 0x0205, [UIntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    # Covers config watcher debounce and the old 1.2-second scale anchor window.
    Start-Sleep -Milliseconds 1500
}

function Exit-AuditEditModeByCommand([IntPtr]$Hwnd) {
    [CatimePositionAuditNative]::SendMessage(
        $Hwnd, 0x0111, (New-UIntPtr 113), [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 1500
}

function Invoke-AuditDrag(
    [IntPtr]$Hwnd,
    [int]$DeltaX,
    [int]$DeltaY,
    [bool]$ExitWhileHeld
) {
    $rect = Get-AuditRect $Hwnd
    $startX = $rect.Left + [int](($rect.Right - $rect.Left) / 2)
    $startY = $rect.Top + [int](($rect.Bottom - $rect.Top) / 2)
    Set-AuditCursor $startX $startY

    [CatimePositionAuditNative]::SendMessage(
        $Hwnd, 0x0201, (New-UIntPtr 1), [IntPtr]::Zero) | Out-Null
    for ($step = 1; $step -le 6; $step++) {
        Set-AuditCursor `
            ($startX + [int]($DeltaX * $step / 6)) `
            ($startY + [int]($DeltaY * $step / 6))
        [CatimePositionAuditNative]::SendMessage(
            $Hwnd, 0x0200, (New-UIntPtr 1), [IntPtr]::Zero) | Out-Null
    }

    if ($ExitWhileHeld) {
        $preExitRect = Get-AuditRect $Hwnd
        Exit-AuditEditMode $Hwnd
    } else {
        $preExitRect = $null
    }

    [CatimePositionAuditNative]::SendMessage(
        $Hwnd, 0x0202, [UIntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 250
    if (-not $preExitRect) {
        $preExitRect = Get-AuditRect $Hwnd
    }
    return $preExitRect
}

function Invoke-AuditWheel([IntPtr]$Hwnd, [int]$Notches) {
    $rect = Get-AuditRect $Hwnd
    Set-AuditCursor `
        ($rect.Left + [int](($rect.Right - $rect.Left) / 2)) `
        ($rect.Top + [int](($rect.Bottom - $rect.Top) / 2))

    for ($index = 0; $index -lt $Notches; $index++) {
        [CatimePositionAuditNative]::SendMessage(
            $Hwnd, 0x020A, (New-UIntPtr 0x00780000), [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 50
    }
    # Drag suppression is 120 ms; the stale anchor used to survive for 1200 ms.
    Start-Sleep -Milliseconds 180
}

function Test-AuditGeometry(
    [IntPtr]$Hwnd,
    [CatimePositionAuditNative+RECT]$ExpectedRect
) {
    $actual = Get-AuditRect $Hwnd
    return [Math]::Abs($actual.Left - $ExpectedRect.Left) -le 1 -and
           [Math]::Abs($actual.Top - $ExpectedRect.Top) -le 1 -and
           [Math]::Abs(($actual.Right - $actual.Left) -
                       ($ExpectedRect.Right - $ExpectedRect.Left)) -le 1 -and
           [Math]::Abs(($actual.Bottom - $actual.Top) -
                       ($ExpectedRect.Bottom - $ExpectedRect.Top)) -le 1
}

function Get-AuditWindowHandle([string]$LogPath, [System.Diagnostics.Process]$Process) {
    $deadline = (Get-Date).AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 100
        if (Test-Path -LiteralPath $LogPath) {
            $match = Select-String -LiteralPath $LogPath `
                -Pattern 'Main window creation successful, handle: 0x([0-9A-Fa-f]+)' |
                Select-Object -Last 1
            if ($match) {
                $hex = $match.Matches[0].Groups[1].Value
                return [IntPtr]([Convert]::ToInt64($hex, 16))
            }
        }
    } while ((Get-Date) -lt $deadline -and -not $Process.HasExited)

    throw "Timed out waiting for the isolated Catime window"
}

function Invoke-PositionAuditIteration([int]$Iteration) {
    $root = Join-Path (Resolve-Path ".").Path `
        (".tmp\position-audit-{0}-{1}" -f `
            [DateTime]::Now.ToString("yyyyMMdd-HHmmssfff"), $Iteration)
    New-Item -ItemType Directory -Force -Path $root | Out-Null

    $previousOverride = $env:CATIME_CONFIG_ROOT
    $env:CATIME_CONFIG_ROOT = $root
    $process = $null
    try {
        $process = Start-Process -FilePath $ExePath `
            -ArgumentList "--ci-smoke", "--ci-exit-ms=60000" `
            -WindowStyle Hidden -PassThru
        $env:CATIME_CONFIG_ROOT = $previousOverride

        $logPath = Join-Path $root "Catime\Catime_Logs.log"
        $hwnd = Get-AuditWindowHandle $logPath $process
        [CatimePositionAuditNative]::SetWindowPos(
            $hwnd, [IntPtr](-1), 300, 300, 0, 0, 0x0041) | Out-Null
        Start-Sleep -Milliseconds 350

        Enter-AuditEditMode $hwnd
        $base = Get-AuditRect $hwnd
        $dragPreExit = Invoke-AuditDrag $hwnd 140 90 $false
        $dragMoved = $dragPreExit.Left -ne $base.Left -or
                     $dragPreExit.Top -ne $base.Top
        Exit-AuditEditMode $hwnd
        $dragExit = $dragMoved -and
                    (Test-AuditGeometry $hwnd $dragPreExit)
        $dragActual = Get-AuditRect $hwnd
        $dragDetail = "moved=$dragMoved preExit=($($dragPreExit.Left),$($dragPreExit.Top)) postExit=($($dragActual.Left),$($dragActual.Top))"

        Enter-AuditEditMode $hwnd
        Invoke-AuditWheel $hwnd 3
        $base = Get-AuditRect $hwnd
        $scaleDragPreExit = Invoke-AuditDrag $hwnd -110 70 $false
        $scaleDragMoved = $scaleDragPreExit.Left -ne $base.Left -or
                          $scaleDragPreExit.Top -ne $base.Top
        Exit-AuditEditMode $hwnd
        $scaleDragExit = $scaleDragMoved -and
                         (Test-AuditGeometry $hwnd $scaleDragPreExit)
        $scaleDragActual = Get-AuditRect $hwnd
        $scaleDragDetail = "moved=$scaleDragMoved preExit=($($scaleDragPreExit.Left),$($scaleDragPreExit.Top)) postExit=($($scaleDragActual.Left),$($scaleDragActual.Top))"

        Enter-AuditEditMode $hwnd
        Invoke-AuditWheel $hwnd 2
        $base = Get-AuditRect $hwnd
        [CatimePositionAuditNative]::SendMessage(
            $hwnd, 0x0100, (New-UIntPtr 0x27), [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 250
        $scaleKeyPreExit = Get-AuditRect $hwnd
        $scaleKeyMoved = $scaleKeyPreExit.Left -ne $base.Left -or
                         $scaleKeyPreExit.Top -ne $base.Top
        Exit-AuditEditMode $hwnd
        $scaleKeyExit = $scaleKeyMoved -and
                        (Test-AuditGeometry $hwnd $scaleKeyPreExit)
        $scaleKeyActual = Get-AuditRect $hwnd
        $scaleKeyDetail = "moved=$scaleKeyMoved preExit=($($scaleKeyPreExit.Left),$($scaleKeyPreExit.Top)) postExit=($($scaleKeyActual.Left),$($scaleKeyActual.Top))"

        Enter-AuditEditMode $hwnd
        $base = Get-AuditRect $hwnd
        $heldDragPreExit = Invoke-AuditDrag $hwnd 80 -55 $true
        $heldDragMoved = $heldDragPreExit.Left -ne $base.Left -or
                         $heldDragPreExit.Top -ne $base.Top
        $heldDragExit = $heldDragMoved -and
                        (Test-AuditGeometry $hwnd $heldDragPreExit)
        $heldDragActual = Get-AuditRect $hwnd
        $heldDragDetail = "moved=$heldDragMoved preExit=($($heldDragPreExit.Left),$($heldDragPreExit.Top)) postExit=($($heldDragActual.Left),$($heldDragActual.Top))"

        Enter-AuditEditMode $hwnd
        $commandBase = Get-AuditRect $hwnd
        $commandPreExit = Invoke-AuditDrag $hwnd 65 35 $false
        $commandMoved = $commandPreExit.Left -ne $commandBase.Left -or
                        $commandPreExit.Top -ne $commandBase.Top
        Exit-AuditEditModeByCommand $hwnd
        $commandExit = $commandMoved -and
                       (Test-AuditGeometry $hwnd $commandPreExit)
        $commandActual = Get-AuditRect $hwnd
        $commandDetail = "moved=$commandMoved preExit=($($commandPreExit.Left),$($commandPreExit.Top)) postExit=($($commandActual.Left),$($commandActual.Top))"

        [CatimePositionAuditNative]::SetWindowPos(
            $hwnd, [IntPtr](-1), -50, 200, 0, 0, 0x0041) | Out-Null
        Start-Sleep -Milliseconds 250
        Enter-AuditEditMode $hwnd
        $negativeBase = Get-AuditRect $hwnd
        $negativePreExit = Invoke-AuditDrag $hwnd 45 30 $false
        $negativeMoved = $negativePreExit.Left -ne $negativeBase.Left -or
                         $negativePreExit.Top -ne $negativeBase.Top
        Exit-AuditEditMode $hwnd
        $negativeExit = $negativeMoved -and
                        (Test-AuditGeometry $hwnd $negativePreExit)
        $negativeActual = Get-AuditRect $hwnd
        $negativeDetail = "moved=$negativeMoved preExit=($($negativePreExit.Left),$($negativePreExit.Top)) postExit=($($negativeActual.Left),$($negativeActual.Top))"

        $taskbarExit = $true
        $taskbarDetail = "taskbar unavailable"
        $taskbar = [CatimePositionAuditNative]::FindWindow("Shell_TrayWnd", $null)
        if ($taskbar -ne [IntPtr]::Zero) {
            $taskbarRect = Get-AuditRect $taskbar
            $windowRect = Get-AuditRect $hwnd
            $windowWidth = $windowRect.Right - $windowRect.Left
            $windowHeight = $windowRect.Bottom - $windowRect.Top
            $screenWidth = [CatimePositionAuditNative]::GetSystemMetrics(0)
            $screenHeight = [CatimePositionAuditNative]::GetSystemMetrics(1)
            if (($taskbarRect.Right - $taskbarRect.Left) -ge
                ($taskbarRect.Bottom - $taskbarRect.Top)) {
                $taskbarX = $taskbarRect.Left + 30
                $taskbarY = if ($taskbarRect.Top -gt $screenHeight / 2) {
                    $taskbarRect.Top - [int]($windowHeight / 2)
                } else {
                    $taskbarRect.Bottom - [int]($windowHeight / 2)
                }
            } else {
                $taskbarY = $taskbarRect.Top + 30
                $taskbarX = if ($taskbarRect.Left -gt $screenWidth / 2) {
                    $taskbarRect.Left - [int]($windowWidth / 2)
                } else {
                    $taskbarRect.Right - [int]($windowWidth / 2)
                }
            }
            [CatimePositionAuditNative]::SetWindowPos(
                $hwnd, [IntPtr](-1), $taskbarX, $taskbarY,
                0, 0, 0x0041) | Out-Null
            Start-Sleep -Milliseconds 250
            Enter-AuditEditMode $hwnd
            Invoke-AuditWheel $hwnd 2
            $taskbarBase = Get-AuditRect $hwnd
            $taskbarPreExit = Invoke-AuditDrag $hwnd 35 20 $false
            $taskbarMoved = $taskbarPreExit.Left -ne $taskbarBase.Left -or
                            $taskbarPreExit.Top -ne $taskbarBase.Top
            Exit-AuditEditMode $hwnd
            $taskbarExit = $taskbarMoved -and
                           (Test-AuditGeometry $hwnd $taskbarPreExit)
            $taskbarActual = Get-AuditRect $hwnd
            $taskbarDetail = "moved=$taskbarMoved preExit=($($taskbarPreExit.Left),$($taskbarPreExit.Top)) postExit=($($taskbarActual.Left),$($taskbarActual.Top))"
        }

        # Exercise the branch where topmost was already enabled before editing.
        [CatimePositionAuditNative]::SendMessage(
            $hwnd, 0x0111, (New-UIntPtr 187), [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 250
        Enter-AuditEditMode $hwnd
        $topmostBase = Get-AuditRect $hwnd
        $topmostPreExit = Invoke-AuditDrag $hwnd -40 25 $false
        $topmostMoved = $topmostPreExit.Left -ne $topmostBase.Left -or
                        $topmostPreExit.Top -ne $topmostBase.Top
        Exit-AuditEditMode $hwnd
        $topmostExit = $topmostMoved -and
                       (Test-AuditGeometry $hwnd $topmostPreExit)
        $topmostActual = Get-AuditRect $hwnd
        $topmostDetail = "moved=$topmostMoved preExit=($($topmostPreExit.Left),$($topmostPreExit.Top)) postExit=($($topmostActual.Left),$($topmostActual.Top))"

        $rapidExit = $true
        $rapidDetail = ""
        for ($rapid = 0; $rapid -lt 5; $rapid++) {
            Enter-AuditEditMode $hwnd
            $rapidBase = Get-AuditRect $hwnd
            $rapidDeltaX = if (($rapid % 2) -eq 0) { 25 } else { -20 }
            $rapidDeltaY = if (($rapid % 2) -eq 0) { 15 } else { -10 }
            $rapidPreExit = Invoke-AuditDrag `
                $hwnd $rapidDeltaX $rapidDeltaY $false
            $rapidMoved = $rapidPreExit.Left -ne $rapidBase.Left -or
                          $rapidPreExit.Top -ne $rapidBase.Top
            if (($rapid % 2) -eq 0) {
                Exit-AuditEditMode $hwnd
            } else {
                Exit-AuditEditModeByCommand $hwnd
            }
            $rapidStable = $rapidMoved -and
                           (Test-AuditGeometry $hwnd $rapidPreExit)
            $rapidExit = $rapidExit -and $rapidStable
            $rapidActual = Get-AuditRect $hwnd
            $rapidDetail += "#$rapid moved=$rapidMoved pre=($($rapidPreExit.Left),$($rapidPreExit.Top)) post=($($rapidActual.Left),$($rapidActual.Top)); "
        }

        [CatimePositionAuditNative]::SendMessage(
            $hwnd, 0x0010, [UIntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        if (-not $process.WaitForExit(5000)) {
            Stop-Process -Id $process.Id -Force
        }

        return [pscustomobject]@{
            Iteration = $Iteration
            DragExit = $dragExit
            ScaleDragExit = $scaleDragExit
            ScaleKeyExit = $scaleKeyExit
            HeldDragRightExit = $heldDragExit
            CommandExit = $commandExit
            NegativeCoordinateExit = $negativeExit
            TaskbarExit = $taskbarExit
            PreEnabledTopmostExit = $topmostExit
            RapidLoopExit = $rapidExit
            Passed = $dragExit -and $scaleDragExit -and
                     $scaleKeyExit -and $heldDragExit -and
                     $commandExit -and $negativeExit -and
                     $taskbarExit -and $topmostExit -and $rapidExit
            DragDetail = $dragDetail
            ScaleDragDetail = $scaleDragDetail
            ScaleKeyDetail = $scaleKeyDetail
            HeldDragDetail = $heldDragDetail
            CommandDetail = $commandDetail
            NegativeDetail = $negativeDetail
            TaskbarDetail = $taskbarDetail
            TopmostDetail = $topmostDetail
            RapidDetail = $rapidDetail
            AuditRoot = $root
        }
    } finally {
        $env:CATIME_CONFIG_ROOT = $previousOverride
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

$originalCursor = New-Object CatimePositionAuditNative+POINT
[CatimePositionAuditNative]::GetCursorPos([ref]$originalCursor) | Out-Null

try {
    $results = for ($iteration = 1; $iteration -le $Iterations; $iteration++) {
        Invoke-PositionAuditIteration $iteration
    }
    $results | Format-List *
    if ($results.Passed -contains $false) {
        exit 1
    }
} finally {
    [CatimePositionAuditNative]::SetCursorPos(
        $originalCursor.X, $originalCursor.Y) | Out-Null
}
