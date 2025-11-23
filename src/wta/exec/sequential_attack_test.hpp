#pragma once
#include "entity_registry.hpp"
#include "task_executor.hpp"

namespace wta::test {

// 启动一个后台线程，让所有 UAV 按顺序打击所有目标
void start_sequential_attack_test();

}
