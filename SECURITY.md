# Security Policy

## Supported Versions

Catime is distributed as a single portable executable (catime.exe) and follows a rolling release model. We provide security fixes through new release versions for:

| Version | Supported          | Update Method |
| ------- | ------------------ | ------------- |
| Latest release | :white_check_mark: | Current version |
| Previous release (if < 30 days old) | :white_check_mark: | Download new release |
| Older versions | :x: | Must update to latest |

**Important**: Since Catime is a portable application (single .exe file), security fixes are delivered as complete new releases rather than patches. Users need to manually download and replace the executable file to receive security updates.

## Application Security Overview

Catime is a privacy-focused countdown and Pomodoro timer written in pure C for Windows. It has been designed with security and privacy as core principles.

> **Note**: Code locations and line numbers referenced in this document are based on Catime v1.2.0. Line numbers may vary slightly in newer versions as the codebase evolves.

### Built-in Security Features

#### Data Handling
- **Local Storage Only**: All data is stored locally in `%LOCALAPPDATA%\Catime\config.ini`
- **No Cloud Sync**: Zero data transmission to external servers (except update checks)
- **UTF-8 Support**: Proper Unicode handling prevents encoding-related vulnerabilities
- **Input Validation**: All user inputs are validated for buffer overflows and malicious content
- **File Path Validation**: Prevents directory traversal and validates file existence

#### Network Security
- **Minimal Network Access**: Only connects to GitHub API for update checking
- **Endpoint Restriction**: Only accesses `https://api.github.com/repos/vladelaina/Catime/releases/latest`
- **HTTPS Only**: All network communications use encrypted connections
- **User Agent**: Clearly identifies as `"Catime Update Checker"`
- **No Telemetry**: Zero analytics, usage statistics, or tracking

#### System Security
- **Privilege Minimization**: Requests only essential system permissions
- **Dangerous Action Filtering**: Automatically blocks dangerous system actions (RESTART/SHUTDOWN/SLEEP → MESSAGE)
- **Single Instance**: Mutex-based protection against multiple instances
- **Exception Handling**: Comprehensive crash reporting for security incidents
- **Thread-Safe Logging**: Prevents race conditions in log operations

#### Memory Safety
- **Buffer Protection**: Bounds checking on all string operations
- **Dynamic Memory Management**: Proper allocation/deallocation patterns
- **String Length Validation**: Prevents buffer overflow vulnerabilities
- **Safe API Usage**: Uses secure Windows API variants (e.g., `_snprintf_s`)

## Potential Security Considerations

### Attack Surface Analysis

The following areas have been identified as the primary attack vectors:

#### 1. Configuration File Processing (`src/config.c`)
- **Risk**: INI file parsing vulnerabilities
- **Mitigation**: Input validation, buffer size checks, UTF-8 encoding
- **Location**: Lines 80-96 in `config.c`

#### 2. Network Update Checking (`src/update_checker.c`)
- **Risk**: Network-based attacks, malicious responses
- **Mitigation**: HTTPS enforcement, JSON parsing validation, response size limits
- **Location**: Lines 496-639 in `update_checker.c`

#### 3. User Input Handling (`src/dialog_procedure.c`)
- **Risk**: Input validation bypasses
- **Mitigation**: Dialog input length limits, character filtering
- **Location**: Lines 110-156 in `window_procedure.c`

#### 4. File Path Operations
- **Risk**: Directory traversal, file system access
- **Mitigation**: Path validation, file existence checks, safe file operations
- **Location**: Lines 1541-1612 in `window_procedure.c`

#### 5. System Integration
- **Risk**: Windows API vulnerabilities, privilege escalation
- **Mitigation**: Minimal permission requests, dangerous action filtering
- **Location**: Lines 1089-1144 in `config.c`

### Security-Relevant Code Locations

For security researchers and auditors, key security implementations can be found at:

```c
// Input validation and buffer protection
src/config.c:80-96       // INI file parsing with UTF-8 support
src/window_procedure.c:110-156  // Dialog input handling

// Network security
src/update_checker.c:496-639    // GitHub API communication
src/update_checker.c:86-200     // JSON response parsing

// System security
src/config.c:1089-1144          // Dangerous action filtering
src/main.c:294-338             // Single instance enforcement
src/log.c:251-279              // Thread-safe logging

// Memory safety
src/update_checker.c:540-595    // Dynamic buffer management
```

## Reporting a Security Vulnerability

We take security vulnerabilities seriously. If you discover a security issue in Catime:

### Do NOT Report Through Public Channels

- ❌ GitHub Issues
- ❌ GitHub Discussions  
- ❌ Public forums or social media

### Report Privately

1. **Preferred Method**: Use GitHub's private vulnerability reporting feature
   - Go to the repository's Security tab
   - Click "Report a vulnerability"
   - Fill out the vulnerability details

2. **Alternative Method**: Create a private discussion
   - Contact the maintainer directly through GitHub

### What to Include

When reporting a vulnerability, please provide:

- **Vulnerability Type**: (e.g., buffer overflow, directory traversal, etc.)
- **Affected Component**: Specific source file and line numbers if known
- **Attack Vector**: How the vulnerability can be exploited
- **Impact Assessment**: Potential consequences of successful exploitation
- **Proof of Concept**: Step-by-step reproduction (if safe to share)
- **Suggested Fix**: If you have ideas for remediation
- **Environment Details**: Windows version, Catime version, etc.

### Response Timeline

We commit to the following response times:

- **Initial Acknowledgment**: Within 48 hours
- **Preliminary Assessment**: Within 5 business days
- **Detailed Analysis**: Within 14 days
- **Fix Implementation**: Within 30 days (for high-severity issues)
- **Public Disclosure**: 90 days after fix release (coordinated disclosure)

### Security Researcher Recognition

We believe in recognizing security researchers who help improve Catime's security:

- Security fixes will acknowledge the reporter (unless anonymity is requested)
- Significant vulnerabilities will be mentioned in release notes
- We maintain a security hall of fame in the project documentation

## Security Best Practices for Users

### Installation Security
- **Download from Official Sources**: Only download from [GitHub Releases](https://github.com/vladelaina/Catime/releases)
- **Verify Checksums**: When available, verify file integrity
- **Avoid Third-Party Builds**: Don't use unofficial or modified versions

### Usage Security
- **Keep Updated**: Enable automatic update checking or check regularly
- **Review Permissions**: Understand what system permissions Catime requests
- **Monitor Log Files**: Check `Catime_Logs.log` for unusual activity
- **Backup Configuration**: Keep a backup of your `config.ini` file

### System Hardening
- **Run as Standard User**: Don't run Catime with elevated privileges
- **Firewall Configuration**: Allow only necessary network access
- **Antivirus Scanning**: Regularly scan the application directory

## Security Architecture

### Threat Model

Catime's threat model considers:

1. **Local Attackers**: Users with physical access to the machine
2. **Network Attackers**: Man-in-the-middle attacks on update checks
3. **Malware**: Other software attempting to exploit Catime
4. **Configuration Tampering**: Unauthorized modification of settings

### Security Boundaries

- **Process Isolation**: Catime runs in its own process space
- **File System Isolation**: Configuration confined to designated directories
- **Network Isolation**: Only GitHub API access permitted
- **UI Isolation**: Dialog inputs are sanitized and validated

## Compliance and Standards

- **OWASP**: Follows secure coding practices from OWASP guidelines
- **Microsoft SDL**: Incorporates Microsoft Security Development Lifecycle practices
- **CWE Mitigation**: Addresses common vulnerability patterns from CWE/SANS Top 25

---

**This security policy is effective as of September 2025 and is reviewed quarterly.**

*For questions about this security policy, please use the contact methods available on the [GitHub repository](https://github.com/vladelaina/Catime).*