#include <regex>

#include "./TileExp/model/hardware.hpp"

namespace Hardware{

namespace ArchTopology{

std::unordered_map<std::string, LevelType> String2LevelType = {
    {"Arith", LevelType::Arith},
    {"Buffer", LevelType::Buffer}
};

std::unordered_map<LevelType, std::string> LevelType2String = {
    {LevelType::Arith, "Arith"},
    {LevelType::Buffer, "Buffer"}
};

void ArchTopo::Print() const{
    std::cout << "****** Print Architencute Topology ******" << std::endl;
    for (auto& pair : archTopo_map_) {
        const std::string& source = pair.first;
        const LevelAttri target = pair.second;

        std::cout << "Level Name: " << source << ", Level Type: " << LevelType2String[target.level_type] << std::endl;
    }
    std::cout << "****** End Print Architencute Topology ******" << std::endl;
}

void ArchTopo::SetLevelType(std::string level_name){
    if (level_name == "DRAM" || level_name == "SRAM" || level_name == "REG") levelAttri_.level_type = LevelType::Buffer;
    else if (level_name == "MAC" || level_name == "VEC") levelAttri_.level_type = LevelType::Arith;
    else TILEEXP_ASSERT(0, "unknown level type");
}

void ArchTopo::SetAttributes(config::CompoundConfigNode config){
    if (config.exists("sizeKB")) config.lookupValue("sizeKB", levelAttri_.sizeKB);
    if (config.exists("word-bits")) config.lookupValue("word-bits", levelAttri_.word_bits);
    if (config.exists("meshX")) config.lookupValue("meshX", levelAttri_.meshX);
    if (config.exists("meshY")) config.lookupValue("meshY", levelAttri_.meshY);
    if (config.exists("depth")) config.lookupValue("depth", levelAttri_.depth);
    if (config.exists("block_size")) config.lookupValue("block_size", levelAttri_.block_size);
    if (config.exists("meshX")) config.lookupValue("meshX", levelAttri_.compute_cycles);
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
    }
}

}

}