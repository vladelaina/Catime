# 番茄钟阶段动作使用说明

番茄钟进入工作中、短休息或长休息时，可以切换时钟文字颜色、播放声音。当前支持“颜色”和“声音”两种动作，每个阶段独立勾选。

## 快速开始

1. 右键点击托盘图标，打开“番茄时钟”。
2. 在“组合”下方打开同级的“状态动作配置”。
3. 选择“工作中动作”“短休息动作”或“长休息动作”，勾选需要的“颜色”和“声音”。三个阶段默认仅勾选“颜色”。
4. 点击番茄时钟菜单中的“开始”。进入阶段时执行该阶段已勾选的动作。
5. 如需修改颜色或声音文件，编辑下文的 `pomodoro_hooks.ini` 并保存；下一次进入对应阶段时生效。

勾选动作会保存到主配置；勾选或取消勾选本身不会立即执行动作。重新开始番茄钟会再次进入工作阶段，可用于查看工作阶段的新配置。

## 默认效果

| 阶段 | 默认颜色 | 声音 |
| --- | --- | --- |
| 工作中 | 当前默认时钟颜色 | 默认关闭；勾选后播放系统提示音 |
| 短休息 | 浅青 → 浅蓝，`#89F7FE_#66A6FF` | 默认关闭；勾选后播放系统提示音 |
| 长休息 | 青绿 → 亮绿，`#10BD92_#8CE442` | 默认关闭；勾选后播放系统提示音 |

阶段颜色仅影响番茄钟当前显示，默认时钟颜色仍会保留。重置或结束番茄钟后恢复默认颜色；浏览色板时，预览颜色会临时优先显示。

## 配置文件位置

阶段配置文件与主 `config.ini` 放在同一目录，通常为：

```text
%LOCALAPPDATA%\Catime\config.ini
%LOCALAPPDATA%\Catime\pomodoro_hooks.ini
```

可以在资源管理器地址栏输入 `%LOCALAPPDATA%\Catime` 打开目录。第一次执行颜色或声音动作时，程序会自动创建缺失的 `pomodoro_hooks.ini`。如果所有阶段动作都已关闭，先勾选一个动作并开始番茄钟，或手动创建该文件。

建议以 UTF-8 保存，尤其是声音路径中含有中文时。默认内容如下，也可参考仓库中的[配置示例](../resource/config/pomodoro_hooks.ini)：

```ini
[Work]
Color=DEFAULT
SoundFile=SYSTEM_BEEP

[ShortBreak]
Color=#89F7FE_#66A6FF
SoundFile=SYSTEM_BEEP

[LongBreak]
Color=#10BD92_#8CE442
SoundFile=SYSTEM_BEEP
```

`[Work]`、`[ShortBreak]`、`[LongBreak]` 分别对应工作中、短休息、长休息。

已有配置文件不会被默认模板覆盖。要在已有文件中采用新的休息渐变色，修改相应的 `Color` 值即可，声音配置可继续保留。

## 设置颜色

只有该阶段勾选“颜色”时，`Color` 才会用于显示。

| 写法 | 示例 | 效果 |
| --- | --- | --- |
| 当前默认颜色 | `Color=DEFAULT` | 跟随当前默认时钟颜色；任何阶段都可以使用 |
| 单一颜色 | `Color=#66A6FF` | 使用指定 HEX 颜色 |
| RGB | `Color=rgb(102,166,255)` | 使用指定 RGB 颜色，也支持 `102,166,255` |
| 渐变 | `Color=#89F7FE_#66A6FF` | 用下划线连接两个颜色 |

不合法或超出长度限制的颜色会回退到该阶段默认颜色。未勾选“颜色”的阶段使用普通默认时钟颜色，不会沿用上一阶段的颜色覆盖。

## 设置声音

只有该阶段勾选“声音”时，`SoundFile` 才会在进入阶段时播放。

三个阶段的默认音源均为系统提示音，无需下载声音文件。旧配置中的 `DEFAULT` 会使用当前通知音；如果主配置的通知音为空，也会回退到系统提示音。如需静音，请将该阶段的 `SoundFile` 留空或取消勾选“声音”。

| 写法 | 效果 |
| --- | --- |
| `SoundFile=SYSTEM_BEEP` | 默认值，播放系统提示音 |
| `SoundFile=DEFAULT` | 使用当前通知设置中的声音；通知音未设置时播放系统提示音 |
| `SoundFile=` | 该阶段进入时静音 |
| `SoundFile=D:\Sounds\work.wav` | 使用绝对路径的声音文件 |
| `SoundFile=sounds\short-break.wav` | 使用相对于 `pomodoro_hooks.ini` 所在目录的路径 |
| `SoundFile=%USERPROFILE%\Music\long-break.mp3` | 支持 Windows 环境变量 |

例如，为两种休息设置不同声音：

```ini
[ShortBreak]
Color=#89F7FE_#66A6FF
SoundFile=sounds\short-break.wav

[LongBreak]
Color=#10BD92_#8CE442
SoundFile=sounds\long-break.wav
```

将声音文件放在 `%LOCALAPPDATA%\Catime\sounds`，并在菜单中分别勾选短休息、长休息的“声音”。路径包含空格或中文时可直接填写，不需要额外加引号。

声音文件不存在或无法播放时，现有播放器会尝试回退到系统提示音。全部番茄钟循环完成后的提示音仍由原有通知设置控制；取消阶段的“声音”或将 `SoundFile` 留空，只控制进入该阶段时的声音。

## 在主配置中设置动作

通常直接用菜单勾选即可。如需手动编辑，动作列表位于 `config.ini` 的 `[Pomodoro]` 节，而颜色和声音文件位于独立的 `pomodoro_hooks.ini`：

```ini
[Pomodoro]
POMODORO_WORK_ACTIONS=COLOR
POMODORO_SHORT_BREAK_ACTIONS=COLOR,SOUND
POMODORO_LONG_BREAK_ACTIONS=COLOR,SOUND
```

`COLOR` 表示颜色，`SOUND` 表示声音；留空表示关闭该阶段全部动作。动作名称不区分大小写，支持逗号或 `|` 分隔，重复动作只执行一次。手动修改主配置后，可重启程序加载；动作在下一次进入阶段时执行。

旧的三种颜色动作 `COLOR_DEFAULT`、`COLOR_LIGHT_GREEN`、`COLOR_GREEN` 会转换为统一的 `COLOR`。`LOG`、`SHAKE` 和其他不支持的动作会被忽略；原列表仅有这些动作时，需要重新勾选要使用的动作。

## 常见问题

- **修改后当前颜色没有立即变化**：阶段配置在进入阶段时读取，等待下一次进入该阶段；菜单勾选变化也按这个时机执行。
- **配置文件不存在**：先勾选颜色或声音并开始番茄钟，或参照模板手动创建。文件应与主配置放在同一目录。
- **声音没有播放或只听到系统提示音**：确认对应阶段已勾选声音、`SoundFile` 不为空、文件存在且可播放；相对路径以配置文件目录为基准。
- **升级后仍显示原来的休息颜色**：已有文件会保留原设置，修改短休息和长休息的 `Color` 即可使用新的默认渐变色。
- **希望执行脚本或其他自定义动作**：当前尚未实现，后续将单独增加自定义动作支持。

本次改动的完整说明见[变更说明](changes/pomodoro-phase-actions.md)。
