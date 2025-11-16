---
description: 新增一个字段信息的全流程
auto_execution_mode: 3
---

当我需要新增一个字段时，需要遵循以下流程，你需要掌握以下信息

# 架构
arma3作为模拟器和运行时，使用intercept c++插件进行控制，intercept有sqf语言的cpp wrap接口，覆盖几乎所有的sqf函数，intercept插件获取到数据后，使用protobuf序列化信息并用Zeromq发送到服务器，zmq服务器将信息转发到前端、求解器和别的应用

# 流程
- 根据我需要的信息，查看intercept库的代码或上网搜索，选择合适的sqf接口
- 根据调研到的一个或多个sqf接口组合，定义protobuf数据结构
- 在cpp插件层调用这些sqf接口，并适配到protobuf和Zeromq
- 如有需要，修改zmq server的ts代码
- 同步修改前端界面的展示内容

