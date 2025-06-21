查看哈希

Get-FileHash "$env:USERPROFILE\Desktop\catime_1.1.0.exe" -Algorithm SHA256




安装 Windows Package Manager（Winget）验证工具

   winget install Microsoft.WingetCreate


验证命令
   winget validate .\manifests\v\VladElaina\Catime\1.1.0\

本地测试验证

winget install -m "C:\Users\vladelaina\Desktop\winget\manifests\v\VladElaina\Catime\1.1.2"

查看下载的位置
where catime

卸载
winget uninstall catime
