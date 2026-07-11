# Scoop packaging

`catime.json` is ready to copy to `bucket/catime.json` in a fork of
[`ScoopInstaller/Extras`](https://github.com/ScoopInstaller/Extras).

Test the local manifest on Windows before opening the pull request:

```powershell
scoop install .\packaging\scoop\catime.json
catime 25m
scoop uninstall catime
```

Catime keeps configuration, logs, fonts, audio, animations, and plugins under
`%LOCALAPPDATA%\Catime`. The manifest intentionally does not declare `persist`
because no user data is stored in the Scoop application directory.

The application-created startup and desktop shortcuts use Scoop's stable
`apps\catime\current\catime.exe` path when it is available, allowing them to
survive upgrades and cleanup of old version directories.
