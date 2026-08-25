# Security Policy

## Reporting Security Issues

We always aim to ship secure software and take security flaws seriously. Thank you for responsibly disclosing your findings to the team to keep the open source community a safe space.

To report a security issue, reach us at vladelaina@gmail.com. We will be in touch should we need any additional information or guidance to fix the bug.

## Antivirus detections and build verification

Catime is a native C/Win32 application, not a .NET/MSIL program. The executable
contains explicit user-facing features that can resemble malware heuristics:
optional Startup-folder shortcut creation, GitHub release update checks,
downloaded Markdown images, and user-approved plugin process launching. Plugin
PowerShell files run under the host PowerShell execution policy; Catime does
not request an execution-policy bypass. These paths are bounded and validated
in the source; Catime does not inject into other processes or install a hidden
service.

Unsigned MinGW builds can also receive reputation-based or heuristic detections;
the existing v1.5.0 release is an older sample and should not be used as the
security baseline for a newly built binary.

The `security / source-policy` job runs before CI and release builds. It blocks
PowerShell execution-policy bypass arguments and high-entropy tokens embedded
in URLs. It also prevents build settings that disable stack protection, strip
all PE metadata, or suppress the PE timestamp. Release builds use conventional
optimization and native Windows resources by default instead of a custom
compressed asset container. These choices keep avoidable heuristic triggers out
of compiled builds; approved short public links remain valid.

The GitHub Actions `security / virustotal` job uploads the final CI artifact and
writes the SHA-256, VirusTotal link, engine counts, and any engine names to the
workflow summary. Configure the repository Actions secret
`VIRUSTOTAL_API_KEY` to enable uploads. If the secret is absent, the GitHub
job records a skipped scan in the Actions Summary and does not block the build
or release. Fork pull requests are excluded from upload jobs and do not receive
a secret.

The VirusTotal report is stored only as an Actions artifact and in the job
summary. It is deliberately not included in the GitHub Release assets.

VirusTotal submissions are visible to VirusTotal's analysis service and public
API keys are rate-limited, so use this workflow only for binaries intended for
public distribution (or an account/plan approved for your release process).

When reporting a suspected false positive, include the exact SHA-256 and the
VirusTotal report URL from the workflow summary. Submit the same hash to the
vendor's false-positive portal; do not rely on a renamed or repackaged file.

