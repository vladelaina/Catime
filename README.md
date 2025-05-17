<p align="center">
<a href="https://github.com/vladelaina/Catime" target="_blank">
<img align="center" alt="catime" width="230" src="Images/catime.png" />
</a>
  
<p align="center">
<a href="https://qm.qq.com/q/LgscIMw9i0" target="_blank">
    <img src="https://img.shields.io/badge/QQ%20Group%20-%20%201027327912-blue?logo=tencentqq&logoColor=white&style=for-the-badge" alt="Join QQ Group" />
</a>

  <a href="https://discord.com/invite/W3tW2gtp6g" target="_blank">
    <img src="https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white&style=for-the-badge" alt="Join Discord" />
  </a>
  <a href="https://vladelaina.github.io/Catime/support" target="_blank">
    <img src="https://img.shields.io/badge/Buy%20me%20a%20coffee-Support%20-critical?logo=buymeacoffee&logoColor=white&style=for-the-badge" alt="Buy me a coffee" />
  </a>

  <br/>

  <a href="https://github.com/vladelaina/Catime/blob/main/LICENSE">
    <img alt="License" src="https://img.shields.io/github/license/vladelaina/Catime?label=License&style=for-the-badge&color=blue" />
  </a>
  <a href="https://github.com/vladelaina/Catime/releases/latest">
    <img alt="Release" src="https://img.shields.io/github/release/vladelaina/Catime.svg?label=Release&style=for-the-badge&color=brightgreen" />
  </a>
  <a href="https://github.com/vladelaina/Catime/releases">
    <img alt="Downloads" src="https://img.shields.io/github/downloads/vladelaina/Catime/total?label=Downloads&style=for-the-badge&color=orange" />
  </a>
  <a href="https://github.com/vladelaina/Catime/releases" target="_blank">
    <img src="https://img.shields.io/badge/Software%20Size-834%20KB-blue?logo=appveyor&logoColor=white&style=for-the-badge" alt="Software Size" />
  </a>
</p>






<h1 align="center">Catime</h1>

<div align="center">
<a href="https://hellogithub.com/repository/00a7a32b7bc647e1a62747530bc16115" target="_blank"><img src="https://api.hellogithub.com/v1/widgets/recommend.svg?rid=00a7a32b7bc647e1a62747530bc16115&claim_uid=JBczix10rXqNblQ" alt="Featured｜HelloGitHub" style="width: 250px; height: 54px;" width="250" height="54" /></a>
</div>

<p align="center">一款简洁的 Windows 倒计时工具，支持番茄时钟功能，具有透明界面和丰富的自定义选项。</p>
<p align="center">A simple Windows countdown tool with Pomodoro clock functionality, featuring a transparent interface and a variety of customization options.</p>


<div align="center">
<video src="https://github.com/user-attachments/assets/2f733378-8ddd-468f-b971-7e98ea4824c2" autoplay loop></video>
</div>

**English** | [**简体中文**](./README-zh.md)

## [🌟Features](https://vladelaina.github.io/Catime/#core-features)

- **Minimalist Design**: Transparent interface, click-through capability, free dragging and resizing, no complex graphical settings interface, won't feel like an additional application running
- **Flexible Timing**: Quick presets, custom time input, current time display (12/24-hour format), countdown, count-up, Pomodoro clock
- **Timeout Actions**: Display messages, show current time, count up, lock screen, open files/applications, open websites, shutdown, restart
- **Rich Customization**: 47 fonts, 15 preset colors, support for custom color values, color palette selection, real-time preview

## [💡Use Cases](https://vladelaina.github.io/Catime/#use-cases)

- 👔 Countdown to end of work
- 🍅 Pomodoro technique
- 🎮 Use while gaming to keep track of time
- ⏰ Schedule opening of:
  - 🌐 Websites
  - 💻 Applications
  - 📄 Files
  - 📝 Batch scripts
  - 🔄 Scheduled shutdown/restart
- 📊 Presentation countdown timer
- 🕰️ Desktop clock
- ⏱️ Count-up/countdown timer

 Demo video (based on V1.0.5): <img src="https://www.bilibili.com/favicon.ico" width="20" height="20"> [BiliBili](https://www.bilibili.com/video/BV1AsoKYgE3r/)

## [📑User Guide](https://vladelaina.github.io/Catime/guide)

- **Left-click tray icon**: Set **time**
- **Right-click tray icon**: Set **menu**
- **Edit mode**: Drag to adjust position, scroll wheel to resize, 🖱️ right-click to exit edit mode




## 🛠️ [Building from Source](https://vladelaina.com/blog-post?slug=Build_catime_from_source)

### 1. Clone:

```
git clone git@github.com:vladelaina/Catime.git
cd Catime
```



### 2. Tools (MinGW, xmake)


#### 🐧Linux

- Ubuntu
   ```bash
   sudo apt update && sudo apt install -y mingw-w64 && curl -fsSL https://xmake.io/shget.text | bash
   ```
- Arch
  ```bash
  sudo pacman -Syu --noconfirm mingw-w64 xmake
  ```

#### 🪟Windows
  
  1. Prepare tools

  | Tool       | Description            | Recommended Version Format                                    | Download Link                                                                     |
  | ---------- | ---------------------- | ------------------------------------------------------------- | --------------------------------------------------------------------------------- |
  | **MinGW**  | GCC Compiler           | `x86_64-<version>-release-win32-seh-ucrt-rtv<num>-rev<num>.7z` | [MinGW Build](https://github.com/niXman/mingw-builds-binaries/releases/latest)  |
  | **xmake**  | Build Tool             | `xmake-v<version>-win64.exe`                                  | [xmake](https://github.com/xmake-io/xmake/releases/latest)                      |

  <details>
    <summary>2. Install tools</summary>

  #### 📦 2.1 Install [MinGW Build](https://github.com/niXman/mingw-builds-binaries/releases/latest)

  1. **Extract MinGW to a specific location**
     Example: Extract `x86_64-<version>-release-win32-seh-ucrt-rtv<num>-rev<num>.7z` to:

     ```
     C:\mingw64
     ```

  2. **Configure system environment variable PATH**

     * Open: `Control Panel → System → Advanced System Settings → Environment Variables`
     * Find `Path` in **System variables**, click "Edit"
     * Add the following path:

       ```
       C:\mingw64\bin
       ```

  3. **Verify installation**

     Open command prompt (Win + R → type `cmd` → Enter), type:

     ```bash
     gcc --version
     ```

     If version number displays successfully, MinGW installation is complete ✅

  #### 📦 2.2 Install [xmake](https://github.com/xmake-io/xmake/releases/latest)

  1. Run `xmake-v<version>-win64.exe` to install
  2. PATH will be configured automatically during installation (if not, manually add the `bin` directory of the xmake installation)

  </details>


### 3. Verify tools
  <details>
    <summary></summary>


  #### ✅ 3.1 Verify gcc

  ```bash
  gcc --version
  ```

  #### ✅ 3.2 Verify xmake

  ```bash
  xmake --version
  ```

  If all display version numbers correctly, tool configuration is successful 🎉
  </details>





### 4. Build with xmake

Open a command prompt in the project root directory and use these commands:

```bash
xmake         # Compile the project
xmake run     # Compile and run the project
xmake clean   # Clean build artifacts
```





## ⭐Star History

<a href="https://star-history.com/#vladelaina/Catime&Date">
    <img src="https://api.star-history.com/svg?repos=vladelaina/Catime&type=Date" height="420" alt="Star History Chart">
</a>


## [✨Acknowledgements](https://vladelaina.github.io/Catime/#thanks) 

Special thanks to the following contributors:
<table>
  <tbody>
    <tr>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/MadMaxChow"><img src="https://avatars.githubusercontent.com/u/13810505?v=4" width="100px;" alt="MAX°孟兆"/><br /><sub><b>MAX°孟兆</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/sumruler"><img src="https://avatars.githubusercontent.com/u/56953545?v=4" width="100px;" alt="XuJilong"/><br /><sub><b>XuJilong</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://www.diandiapp.com/"><img src="https://github.com/user-attachments/assets/ed3ad284-d4f6-456f-a451-0d4c3cba05a4" width="100px;" alt="点滴进度作者"/><br /><sub><b>点滴进度作者</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/ZGGSONG"><img src="https://avatars.githubusercontent.com/u/49741009?v=4" width="100px;" alt="zggsong"/><br /><sub><b>zggsong</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/26087398"><img src="https://i1.hdslb.com/bfs/face/af55083fafbabb7815b09c32adca94139b3ab3f8.webp@240w_240h_1c_1s_!web-avatar-space-header.avif" width="100px;" alt="猫屋敷梨梨Official"/><br /><sub><b>猫屋敷梨梨Official</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/6189012"><img src="https://i0.hdslb.com/bfs/face/e38f4197fddc38397732b61c3086cd6b280dd00e.jpg" width="100px;" alt="MOJIもら"/><br /><sub><b>MOJIもら</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/475437261"><img src="https://i0.hdslb.com/bfs/face/a52c54f0098602b2934d828222aaf3895b06c9ec.jpg@240w_240h_1c_1s_!web-avatar-space-header.avif" width="100px;" alt="李康"/><br /><sub><b>李康</b></sub></a><br />
      </td>
    </tr>
    <tr>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/1708573954"><img src="https://i1.hdslb.com/bfs/face/7fe7cfba25dd086f9b4dbb8433b5db237a5ff98b.jpg@240w_240h_1c_1s_!web-avatar-space-header.avif" width="100px;" alt="我是无名吖"/><br /><sub><b>我是无名吖</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/flying-hilichurl"><img src="https://github.com/user-attachments/assets/e83d0c15-cfe1-46d0-9904-c4dfda7baf0a" width="100px;" alt="金麟"/><br /><sub><b>金麟</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/161061562"><img src="https://i1.hdslb.com/bfs/face/ffbffc12d4cb51d158210f26f45bb1b369eaf730.jpg@240w_240h_1c_1s_!web-avatar-space-header.avif" width="100px;" alt="双脚猫"/><br /><sub><b>双脚猫</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/rsyqvthv"><img src="https://avatars.githubusercontent.com/u/2230369?v=4" width="100px;" alt="rsyqvthv"/><br /><sub><b>rsyqvthv</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/297146893"><img src="https://github.com/user-attachments/assets/9b6a9293-c0f7-4c03-a101-eff670dd0154" width="100px;" alt="君影"/><br /><sub><b>君影</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/3546380188519387"><img src="https://i1.hdslb.com/bfs/face/a6396d677f543a173aa11f3d3cd2943a96121dda.jpg@240w_240h_1c_1s_!web-avatar-space-header.avif" width="100px;" alt="学习马楼"/><br /><sub><b>学习马楼</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/8010065"><img src="https://i2.hdslb.com/bfs/face/118e5f530477e11326dbfb3692a0878fc92d303d.jpg@240w_240h_1c_1s_!web-avatar-space-header.avif" width="100px;" alt="睡着的火山"/><br /><sub><b>睡着的火山</b></sub></a><br />
      </td>
    </tr>
    <tr>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/5549978"><img src="https://i2.hdslb.com/bfs/face/8da0d17a9d45bb66fb8758c4da378a145e1856ca.jpg@240w_240h_1c_1s_!web-avatar-space-header.avif" width="100px;" alt="星空下数羊"/><br /><sub><b>星空下数羊</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/13129221"><img src="https://i2.hdslb.com/bfs/face/5a3c76d35af300d44111a50bde4b7acf45ad9621.jpg@128w_128h_1c_1s.webp" width="100px;" alt="青阳忘川"/><br /><sub><b>青阳忘川</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/llfWilliam"><img src="https://avatars.githubusercontent.com/u/192875064?v=4" width="100px;" alt="William"/><br /><sub><b>William</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/wangye99"><img src="https://avatars.githubusercontent.com/u/59310777?v=4" width="100px;" alt="王野"/><br /><sub><b>王野</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/572042200"><img src="https://i1.hdslb.com/bfs/face/b952499e109734d73d81a2a6929aefd30b3fbc9d.png@128w_128h_1c_1s.webp" width="100px;" alt="煮酒论科技"/><br /><sub><b>煮酒论科技</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/470931145"><img src="https://i2.hdslb.com/bfs/face/bd8660b3927b5be8fc724c2cf9be3d8ebe44ffa2.jpg@128w_128h_1c_1s.webp" width="100px;" alt="风增"/><br /><sub><b>风增</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/266931550"><img src="https://github.com/user-attachments/assets/2fa88218-ee70-4a99-a225-ec632cfabf23" width="100px;" alt="田春"/><br /><sub><b>田春</b></sub></a><br />
      </td>
    </tr>
    <tr>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/behappy425"><img src="https://avatars.githubusercontent.com/u/115355212?v=4" width="100px;" alt="behappy425"/><br /><sub><b>behappy425</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/xxtui-main/xxtui"><img src="https://avatars.githubusercontent.com/u/31737411?v=4" width="100px;" alt="x.j"/><br /><sub><b>x.j</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/kissholic"><img src="https://avatars.githubusercontent.com/u/26087790?v=4" width="100px;" alt="kissholic"/><br /><sub><b>kissholic</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href=""><img src="https://github.com/user-attachments/assets/ef1ff137-7430-420c-8142-562a48803f56" width="100px;" alt="Alnilam"/><br /><sub><b>Alnilam</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/pyqmlp"><img src="https://github.com/user-attachments/assets/fb0473e9-db9d-4f63-ac8d-4aa1d9ee7579" width="100px;" alt="柒"/><br /><sub><b>柒</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/Mapaler"><img src="https://avatars.githubusercontent.com/u/6565860?v=4" width="100px;" alt="枫谷剑仙"/><br /><sub><b>枫谷剑仙</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/Crazy-A2"><img src="https://avatars.githubusercontent.com/u/68056912?v=4" width="100px;" alt="Marial"/><br /><sub><b>Marial</b></sub></a><br />
      </td>
    </tr>
    <tr>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/507572706"><img src="https://github.com/user-attachments/assets/8bf7a9e8-749a-47b6-976c-6798bacea6d2" width="100px;" alt="卓越方达"/><br /><sub><b>卓越方达</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/choyy"><img src="https://avatars.githubusercontent.com/u/68107073?v=4" width="100px;" alt="choyy"/><br /><sub><b>choyy</b></sub></a><br />
      </td>
       </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://space.bilibili.com/378034263?spm_id_from=333.1007.0.0"><img src="https://github.com/user-attachments/assets/ec961664-258b-404f-8f9b-f4b396208473" width="100px;" alt="益渊Yyuan"/><br /><sub><b>益渊Yyuan</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/xiaodao0036"><img src="https://avatars.githubusercontent.com/u/173059852?v=4" width="100px;" alt="上条教主"/><br /><sub><b>上条教主</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/jin-gubang"><img src="https://avatars.githubusercontent.com/u/170781370?v=4" width="100px;" alt="jin-gubang"/><br /><sub><b>jin-gubang</b></sub></a><br />
      </td>
      <td align="center" valign="top" width="14.28%">
        <a href="https://github.com/xlr134"><img src="https://avatars.githubusercontent.com/u/138568644?v=4" width="100px;" alt="xlr134"/><br /><sub><b>xlr134</b></sub></a><br />
      </td>
    </tr>  
  </tbody>
</table>



## 🗝️Copyright Notice

This project is released under the Apache License 2.0 open source license.

Icon copyright notice:
- The software icon was created by [猫屋敷梨梨Official](https://space.bilibili.com/26087398) who reserves all rights. Without explicit authorization from the original creator, it may not be used for any commercial purposes.

Font licenses:
- MIT: 
  - ProFont IIx Nerd Font.ttf
- SIL Open Font License (OFL): 
  - DaddyTimeMono Nerd Font Propo Essence.ttf, DepartureMono Nerd Font Propo Essence.ttf,
    LiterationSerif Nerd Font Essence.ttf, RecMonoCasual Nerd Font Mono Essence.ttf,
    Terminess Nerd Font Propo Essence.ttf, VictorMono NFP Medium Essence.ttf,
    ZedMono NF Essence.ttf
  - Arbutus Essence.ttf, Berkshire Swash Essence.ttf, Caveat Brush Essence.ttf,
    Creepster Essence.ttf, DotGothic16 Essence.ttf, Doto ExtraBold Essence.ttf,
    Foldit SemiBold Essence.ttf, Fredericka the Great Essence.ttf, Frijole Essence.ttf,
    Gwendolyn Essence.ttf, Handjet Essence.ttf, Inknut Antiqua Medium Essence.ttf,
    Jacquard 12 Essence.ttf, Jacquarda Bastarda 9 Essence.ttf, Kavoon Essence.ttf,
    Kumar One Essence.ttf, Kumar One Outline Essence.ttf, Lakki Reddy Essence.ttf,
    Licorice Essence.ttf, Ma Shan Zheng Essence.ttf, Moirai One Essence.ttf,
    Mystery Quest Essence.ttf, Noto Nastaliq Urdu Medium Essence.ttf, Piedra Essence.ttf,
    Pixelify Sans Medium Essence.ttf, Press Start 2P Essence.ttf,
    Rubik Bubbles Essence.ttf, Rubik Burned Essence.ttf, Rubik Glitch Essence.ttf,
    Rubik Marker Hatch Essence.ttf,
    Rubik Puddles Essence.ttf, Rubik Vinyl Essence.ttf, Rubik Wet Paint Essence.ttf,
    Ruge Boogie Essence.ttf, Sevillana Essence.ttf, Silkscreen Essence.ttf,
    Stick Essence.ttf, Underdog Essence.ttf, Wallpoet Essence.ttf,
    Yesteryear Essence.ttf, ZCOOL KuaiLe Essence.ttf

Wallpaper:
- Author: [猫屋敷梨梨Official](https://space.bilibili.com/26087398)
    - Dynamic wallpaper link: [Wallpaper Engine](https://steamcommunity.com/sharedfiles/filedetails/?id=3171487185)
---

<div align="center">

Copyright © 2025 - **Catime**\
By vladelaina\
Made with ❤️ & ⌨️

</div>


