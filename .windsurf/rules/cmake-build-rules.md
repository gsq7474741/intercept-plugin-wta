---
trigger: manual
description: 需要构建cpp时启用该规则
---

### 构建

当执行构建或cmake命令时，需要先激活vs环境，使用powershell，激活命令如下

```powershell
Import-Module "D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"; Enter-VsDevShell 97c66f96 -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" 你的构建命令
```

新增功能后你需要自行构建测试直到编译通过，只要需要启动游戏和人工干预时再停下来找我，否则一直运行

编译时提示目标文件在占用可能是游戏正在运行，你需要杀掉游戏进程`arma3_x64.exe`，然后再次运行build命令