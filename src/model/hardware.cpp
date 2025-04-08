#include <regex>

#include "./TileExp/common.hpp"
#include "./TileExp/model/hardware.hpp"

namespace Hardware{

namespace ArchTopology{

std::unordered_map<std::string, LevelType> String2LevelType = {
    {"Matrix", LevelType::Matrix},
    {"Vectpr", LevelType::Vector},
    {"Buffer", LevelType::Buffer}
};

std::unordered_map<LevelType, std::string> LevelType2String = {
    {LevelType::Matrix, "Matrix"},
    {LevelType::Vector, "Vector"},
    {LevelType::Buffer, "Buffer"}
};


void ArchTopo::Print() const{
    std::cout << "****** Print Architencute Topology ******" << std::endl;
    for (auto& pair : archTopo_map_) {
        const std::string& source = pair.first;
        const LevelAttri target = pair.second;

        auto format_name = format_fixed(source, 12);
        auto format_level_name = format_fixed(LevelType2String[target.level_type], 7);
        auto format_level_num = format_fixed(std::to_string(target.level_num), 5);

        std::cout << "Level Name: " << format_name << ", Level Type: " << format_level_name << ", Num: " << format_level_num << std::endl;
    }
    std::cout << "****** End Print Architencute Topology ******" << std::endl;
}

void ArchTopo::SetLevelType(std::string level_name){
    if (level_name == "DRAM" || level_name == "SRAM" || level_name == "REG") levelAttri_.level_type = LevelType::Buffer;
    else if (level_name == "MAC" || level_name == "MATRIX") levelAttri_.level_type = LevelType::Matrix;
    else if (level_name == "VEC" || level_name == "VECTOR") levelAttri_.level_type = LevelType::Vector;
    else TILEEXP_ASSERT(0, "unknown level type");
}

void ArchTopo::SetAttributes(config::CompoundConfigNode config){
    if (config.exists("sizeKB")) config.lookupValue("sizeKB", levelAttri_.sizeKB);
    if (config.exists("word-bits")) config.lookupValue("word-bits", levelAttri_.word_bits);
    if (config.exists("meshX")) config.lookupValue("meshX", levelAttri_.meshX);
    if (config.exists("meshY")) config.lookupValue("meshY", levelAttri_.meshY);
    if (config.exists("depth")) config.lookupValue("depth", levelAttri_.depth);
    if (config.exists("block_size")) config.lookupValue("block_size", levelAttri_.block_size);
    if (config.exists("compute_cycles")) config.lookupValue("compute_cycles", levelAttri_.compute_cycles);
}

void ParseArchTopology(config::CompoundConfigNode config, ArchTopo& archTopo){
    float version;
    config.lookupValue("version", version);

    auto config_ = config.lookup("subtree");
    assert(config_.isList());

    for (int i = 0; i < config_.getLength(); i++){
        archTopo.levelAttri_.clear();
        auto config_node = config_[i];
        std::string name, level_name;
        unsigned int level_num;
        config_node.lookupValue("name", name);
        config_node.lookupValue("level_name", level_name);
        config_node.lookupValue("level_num", level_num);
        archTopo.levelAttri_.name = name; 
        archTopo.levelAttri_.level_name = level_name;
        archTopo.levelAttri_.level_num = level_num;
        archTopo.SetLevelType(level_name);
        auto attributes = config_node.lookup("attributes");
        archTopo.SetAttributes(attributes);
        archTopo.AddArchMap(name);

        archTopo.archTopoName2Idx_map_[name] = archTopo.level_num_;
        archTopo.level_num_ += 1;
    }

    // archTopo.level_num_ = config_.getLength();
}

}

}