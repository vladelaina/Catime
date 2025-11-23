# Privacy Policy

## Introduction

**Catime** is a desktop application focused on personal time management. We highly value your privacy, and this policy explains how we handle your data.

## Data Collection and Usage

### Information We Do **Not** Collect

Catime explicitly commits to:

* **Not** creating any unique identifiers, trackers, or similar tools
* **Not** collecting usage statistics, performance metrics, or any user behavior information
* **Not** uploading any user or system data to remote servers
* **Not** incorporating advertisements or analytics tools
* **Not** selling or sharing your personal data with third parties

### Locally Stored Data

Catime only saves the following information locally:

1. **Configuration Data**
   Includes interface settings, timer configurations, and user preferences, stored in `%LOCALAPPDATA%\Catime\config.ini`.

2. **Log Information**
   Saved in the `Catime_Logs.log` file, containing:

   * Basic system information (OS version, CPU architecture, memory capacity, etc.)
   * Program startup and operation records
   * Error and warning logs
     *All logs are used for local troubleshooting only and are **never** uploaded.*

3. **Recent Files List**
   Stores paths of recently opened files for quick access.

4. **Secure Credentials**
   If you use the Data Monitor feature and provide access tokens (e.g., GitHub Token), these are stored securely using the **Windows Credential Manager**. They are encrypted by the operating system and are **never** saved in plain text configuration files.

### Network Access Information

Catime only accesses the internet in the following circumstances:

* **Update Checking**
  When you manually check for updates or enable the automatic update feature, the program connects to the GitHub API:
  `https://api.github.com/repos/vladelaina/Catime/releases/latest`
  It only sends basic HTTP requests and the user agent string `"Catime Update Checker"`, **without any personal data**.

* **Data Monitor**
  If you configure a monitor (e.g., GitHub Stars), the application connects directly to the respective service API (e.g., `api.github.com`) to fetch data.
  * Requests are made directly from your computer to the service provider.
  * **All network communications are encrypted using HTTPS (TLS/SSL).**
  * Authentication tokens are used solely for these requests and are never sent to any other third-party servers.

## Data Protection and Deletion

All data is stored locally, and Catime does not perform any remote synchronization or uploads.
To completely remove data, you can manually delete the configuration files and log files.

## Permission Usage Information

System permissions requested by Catime are only used to implement core functions:

* **File System Access**: For reading and writing configuration and log files
* **Network Access**: Only for update checking
* **Reading System Information**: Recorded to local log files
* **Startup Item Modification**: Only used when the "Start with System" feature is enabled

## Privacy Policy Changes

If there are significant changes, we will post notifications within the application or on the project homepage.

## Contact Information

If you have questions or suggestions about this privacy policy, please contact us through the GitHub project page.

> Last Updated: November 23, 2025



