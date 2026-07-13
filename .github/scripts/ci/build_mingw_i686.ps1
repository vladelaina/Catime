param(
    [string]$BuildDirectory = "build"
)

$ErrorActionPreference = "Stop"

if (-not $env:WINLIBS_BIN) {
    throw "WINLIBS_BIN is not set"
}

& node -e "const expected='1.3.1-e00f703'; if (process.versions.zlib !== expected) { console.error('Unexpected zlib: ' + process.versions.zlib); process.exit(1); }"
if ($LASTEXITCODE -ne 0) {
    throw "Node.js zlib verification failed"
}

$gcc = (Join-Path $env:WINLIBS_BIN "gcc.exe").Replace('\', '/')
$windres = (Join-Path $env:WINLIBS_BIN "windres.exe").Replace('\', '/')
$make = (Join-Path $env:WINLIBS_BIN "mingw32-make.exe").Replace('\', '/')

& cmake -S . -B $BuildDirectory -G "MinGW Makefiles" `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DCMAKE_C_COMPILER=$gcc" `
    "-DCMAKE_RC_COMPILER=$windres" `
    "-DCMAKE_MAKE_PROGRAM=$make"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed"
}

& cmake --build $BuildDirectory --parallel 4
if ($LASTEXITCODE -ne 0) {
    throw "MinGW i686 build failed"
}

$executable = Join-Path $BuildDirectory "catime.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Built executable was not found: $executable"
}

$objdump = Join-Path $env:WINLIBS_BIN "objdump.exe"
$peHeader = (& $objdump -f $executable | Out-String)
Write-Host $peHeader
if ($LASTEXITCODE -ne 0 -or $peHeader -notmatch "file format pei-i386") {
    throw "Release binary must be 32-bit Win32 (pei-i386)"
}

$file = Get-Item -LiteralPath $executable
$sizeKB = "{0:F2}" -f ($file.Length / 1KB)
$sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
Write-Host "Unsigned x86 EXE size: $sizeKB KB"
Write-Host "SHA-256: $sha256"

$smoke = Start-Process -FilePath $file.FullName `
    -ArgumentList "--ci-smoke --ci-exit-ms=1500" `
    -WindowStyle Hidden -Wait -PassThru
if ($smoke.ExitCode -ne 0) {
    throw "Win32 smoke test failed with exit code $($smoke.ExitCode)"
}
Write-Host "Win32 smoke test passed"
