#include "world_config.hpp"
#include <intercept.hpp>
#include <sstream>

namespace wta {

using namespace intercept;  // 访问 sqf 命名空间

bool WorldConfig::load_from_sqf() {
    try {
        clear();
        
        // 加载平台配置：WTA_PLATFORM_CONFIGS (暂时跳过，因为 HashMap 支持复杂)
        // TODO: 实现平台配置加载
        
        // 加载目标配置：WTA_TARGET_CONFIGS
        // 格式：[[id, type_name, value, tier, [prerequisites]], ...]
        auto target_configs_var = sqf::get_variable(sqf::mission_namespace(), "WTA_TARGET_CONFIGS");
        if (target_configs_var.is_null()) {
            sqf::diag_log("WTA Config: WTA_TARGET_CONFIGS not found");
            loaded_ = false;
            return false;
        }
        
        // 检查是否为数组类型
        if (target_configs_var.type_enum() == types::game_data_type::ARRAY) {
            try {
                auto& configs_array = target_configs_var.to_array();
                
                for (size_t i = 0; i < configs_array.size(); ++i) {
                    auto config_entry = configs_array[i];
                    
                    // 检查子项是否为数组
                    if (config_entry.type_enum() != types::game_data_type::ARRAY) continue;
                    
                    try {
                        auto& entry_array = config_entry.to_array();
                        if (entry_array.size() < 5) continue;
                        
                        TargetConfig cfg;
                        
                        // [0]: id
                        if (entry_array[0].type_enum() == types::game_data_type::SCALAR) {
                            cfg.id = static_cast<int>(static_cast<float>(entry_array[0]));
                        } else {
                            continue;
                        }
                        
                        // [1]: type_name
                        if (entry_array[1].type_enum() == types::game_data_type::STRING) {
                            cfg.type_name = static_cast<std::string>(entry_array[1]);
                        } else {
                            cfg.type_name = "Unknown";
                        }
                        
                        // [2]: value
                        if (entry_array[2].type_enum() == types::game_data_type::SCALAR) {
                            cfg.value = static_cast<float>(entry_array[2]);
                        } else {
                            cfg.value = 1.0f;
                        }
                        
                        // [3]: tier
                        if (entry_array[3].type_enum() == types::game_data_type::SCALAR) {
                            cfg.tier = static_cast<int>(static_cast<float>(entry_array[3]));
                        } else {
                            cfg.tier = 0;
                        }
                        
                        // [4]: prerequisites (array of IDs)
                        if (entry_array[4].type_enum() == types::game_data_type::ARRAY) {
                            try {
                                auto& prereq_array = entry_array[4].to_array();
                                for (size_t j = 0; j < prereq_array.size(); ++j) {
                                    if (prereq_array[j].type_enum() == types::game_data_type::SCALAR) {
                                        cfg.prerequisites.push_back(static_cast<int>(static_cast<float>(prereq_array[j])));
                                    }
                                }
                            } catch (...) {
                                // 前置条件数组解析失败，使用空数组
                            }
                        }
                        
                        target_configs_[cfg.id] = cfg;
                        
                        // 日志
                        std::stringstream ss;
                        ss << "WTA Config: Loaded target " << cfg.id 
                           << " '" << cfg.type_name << "' "
                           << "(value=" << cfg.value 
                           << ", tier=" << cfg.tier 
                           << ", prereqs=" << cfg.prerequisites.size() << ")";
                        sqf::diag_log(ss.str());
                    } catch (...) {
                        // 单个配置项解析失败，跳过
                        continue;
                    }
                }
            } catch (...) {
                sqf::diag_log("WTA Config: Failed to parse WTA_TARGET_CONFIGS array");
                loaded_ = false;
                return false;
            }
        }
        
        loaded_ = true;
        
        std::stringstream summary;
        summary << "WTA Config: Loaded " << target_configs_.size() << " target configs";
        sqf::diag_log(summary.str());
        
        return true;
        
    } catch (const std::exception& e) {
        std::stringstream ss;
        ss << "WTA Config: Error loading config: " << e.what();
        sqf::diag_log(ss.str());
        loaded_ = false;
        return false;
    }
}

std::optional<PlatformConfig> WorldConfig::get_platform_config(const std::string& vehicle_class) const {
    auto it = platform_configs_.find(vehicle_class);
    if (it != platform_configs_.end()) {
        return it->second;
    }
    
    // 如果没有找到配置，返回默认值
    PlatformConfig default_cfg;
    default_cfg.vehicle_class = vehicle_class;
    default_cfg.max_range_km = 10.0f;
    default_cfg.hit_prob = 0.7f;
    default_cfg.cost = 5.0f;
    default_cfg.target_types = {1, 2, 3, 4, 5}; // 默认可打击所有目标
    default_cfg.max_targets = 1;
    
    return default_cfg;
}

std::optional<TargetConfig> WorldConfig::get_target_config(int target_id) const {
    auto it = target_configs_.find(target_id);
    if (it != target_configs_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void WorldConfig::clear() {
    platform_configs_.clear();
    target_configs_.clear();
    loaded_ = false;
}

} // namespace wta
