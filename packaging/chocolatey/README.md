# Chocolatey packaging

Catime is published to the Chocolatey Community Repository as the `catime`
package. Installation and upgrades preserve configuration and user resources
under `%LOCALAPPDATA%\Catime`.

## Local package build

On Windows with Chocolatey installed:

```powershell
.\packaging\chocolatey\update-package.ps1 `
  -Version 1.5.0 `
  -Checksum f89c30c2d076d22870eceee79672d9ed0f5742f994c624a9bb1a7dd61bea1e8d

choco pack .\packaging\chocolatey\catime.nuspec `
  --output-directory .\chocolatey-output
```

Test the generated package with:

```powershell
choco install catime --source .\chocolatey-output --version 1.5.0 --yes --force
catime 25m
choco uninstall catime --yes
```

## Publishing

The Chocolatey workflow can be triggered manually for the first publication.
Stable tag releases call the same workflow automatically after the GitHub
Release has been created. The `CHOCOLATEY_API_KEY` repository secret is passed
only to the final `choco push` step.
