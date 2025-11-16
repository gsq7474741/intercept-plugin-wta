#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace wta {

/**
 * 平台类型配置
 * 对应 SQF: WTA_PLATFORM_CONFIGS
 */
struct PlatformConfig {
    std::string vehicle_class;      // 车辆类名，如 "B_UAV_02_dynamicLoadout_F"
    float max_range_km;              // 最大射程（km）
    float hit_prob;                  // 命中概率 [0-1]
    float cost;                      // 消耗成本
    std::vector<int> target_types;   // 可打击的目标ID列表
    int max_targets;                 // 最多打击目标数
};

/**
 * 目标配置
 * 对应 SQF: WTA_TARGET_CONFIGS
 */
struct TargetConfig {
    int id;                          // 目标ID
    std::string type_name;           // 目标类型名称，如 "预警雷达站"
    float value;                     // 预期价值
    int tier;                        // 目标层级 (0/1/2)
    std::vector<int> prerequisites;  // 前置目标ID列表
};

/**
 * 世界配置管理器
 * 从 SQF 加载平台和目标配置
 */
class WorldConfig {
public:
    WorldConfig() = default;
    
    /**
     * 从 SQF 全局变量加载配置
     * 读取 WTA_PLATFORM_CONFIGS 和 WTA_TARGET_CONFIGS
     * 
     * @return 是否加载成功
     */
    bool load_from_sqf();
    
    /**
     * 获取平台配置
     * 
     * @param vehicle_class 车辆类名
     * @return 配置对象（如果不存在返回默认值）
     */
    std::optional<PlatformConfig> get_platform_config(const std::string& vehicle_class) const;
    
    /**
     * 获取目标配置
     * 
     * @param target_id 目标ID
     * @return 配置对象（如果不存在返回空）
     */
    std::optional<TargetConfig> get_target_config(int target_id) const;
    
    /**
     * 检查是否已加载配置
     */
    bool is_loaded() const { return loaded_; }
    
    /**
     * 清空配置
     */
    void clear();

private:
    bool loaded_ = false;
    std::unordered_map<std::string, PlatformConfig> platform_configs_;
    std::unordered_map<int, TargetConfig> target_configs_;
};

} // namespace wta
