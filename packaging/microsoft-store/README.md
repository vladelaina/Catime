# Microsoft Store packaging

The release workflow uploads `catime_<version>_x86.msix` as the raw
`catime-store-msix-<version>` GitHub Actions artifact. Daily builds append the
GitHub run number to the artifact name. The MSIX is ready for manual upload to
Partner Center, but is not attached to the public GitHub Release and is not
submitted or published automatically.

## Store identity

- Package name: `vladelaina.Catime`
- Publisher: `CN=5503A135-7FA4-466B-815C-DBE627F4065F`
- Publisher display name: `vladelaina`
- Package family name: `vladelaina.Catime_hnew8t3b8e0t6`
- Store ID: `9N3MZDF1Z34V`
- Store URL: <https://apps.microsoft.com/detail/9N3MZDF1Z34V>

These values are public product identity information, not secrets. The package
name and publisher are stored in `AppxManifest.xml.in`. The PFN and package SID
are derived by Windows and must not be used as signing secrets.

The build script creates a temporary self-signed certificate whose subject
matches the manifest publisher. The certificate exists only during the build.
Microsoft Store re-signs accepted MSIX packages during publishing. A private
PFX is only needed when distributing the MSIX outside Microsoft Store.

## App data isolation

Catime uses the normal `%LOCALAPPDATA%\Catime` folder for ordinary EXE,
WinGet, and Scoop installations. For an MSIX-packaged build, Catime resolves
the existing package virtualization location explicitly as
`%LOCALAPPDATA%\Packages\<PFN>\LocalCache\Local\Catime`. This keeps existing
Store data compatible and ensures Explorer opens the same physical folders
that the application reads and writes.

At runtime Catime uses `GetCurrentPackageFullName` to distinguish packaged and
unpackaged execution. The detected mode and PFN are written to the Catime log.

## Local package build

Run on Windows with the Windows 10/11 SDK installed:

```powershell
.\packaging\microsoft-store\build-store-package.ps1 `
  -ExecutablePath .\build\catime.exe `
  -Version 1.5.0
```

The signed x86 MSIX is placed in `output\microsoft-store` by default and can be
uploaded directly to Partner Center. A separate `.msixupload` wrapper is not
created because the current MinGW release build does not produce PDB symbols or
an `.appxsym` file.

The package version is derived as `Major.Minor.Patch.0`; the fourth component
must remain zero for Microsoft Store submissions. Before uploading, confirm
that this version is higher than the package version currently published in
Partner Center.
