---
trigger: always_on
---

## overview
- 这是一个基于arma3的wta问题模拟项目
- 包括
    - 插件：基于intercept和cpp+cmake
    - 求解器：python实现
    - 前端看板：next+ts
- 各个组件之间通过protobuf和Zeromq交换数据
- 使用windsurf ide进行开发（是vscode套壳）
    - 插件：cmake、clangd、clang-format

## 插件
 
- 使用cmake构建，配置在CMakePresets.json里
- 使用ninja生成器
- 使用vs 2022的msvc编译器，编译到64位

### 构建

当执行构建或cmake命令时，需要先激活vs环境，使用powershell，激活命令如下

Import-Module "D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"; Enter-VsDevShell 97c66f96

新增功能后你需要自行构建测试直到编译通过，只要需要启动游戏和人工干预时再停下来找我，否则一直运行

编译时提示目标文件在占用可能是游戏正在运行，你需要杀掉游戏进程`arma3_x64.exe`，然后再次运行build命令



## 前端

- dashboard，用于实时从全局视角监控战局
- 有一个zmq服务器

## 求解器

- python实现的wta问题求解器
- 基于bpso和贪心算法