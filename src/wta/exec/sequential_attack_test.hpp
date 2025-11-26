#pragma once
#include "entity_registry.hpp"
#include "task_executor.hpp"

namespace wta::test {

// 启动集中火力测试：所有 UAV 攻击同一目标，摧毁后切换到下一个
void start_sequential_attack_test();

// 停止测试
void stop_sequential_attack_test();

// 检查测试是否在运行
bool is_test_running();

}
