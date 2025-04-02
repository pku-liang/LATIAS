#pragma once

#include "mapping/parser.hpp"
#include "./TileExp/common.hpp"

namespace Hardware{

namespace ArchTopology{

enum LevelType{
    Arith,
    Buffer,
    UNKNOWN
};

struct LevelAttri{
    std::string name;
    unsigned int level_num;
    std::string level_name;
    LevelType level_type;
    // attributes
    unsigned int sizeKB;
    unsigned int word_bits;
    unsigned int meshX;
    unsigned int meshY;
    unsigned int depth;
    unsigned int block_size;
    unsigned int compute_cycles;

    void clear(){
        name.clear();
        level_name.clear();
        level_type = UNKNOWN;
        level_num = sizeKB = word_bits = meshX = meshY = depth = block_size = compute_cycles = 0;
    }
};

class ArchTopo{
public:
    LevelAttri levelAttri_;
    std::unordered_map<std::string, LevelAttri> archTopo_map_;

    void Print() const;
    void SetLevelType(std::string level_name);
    void SetAttributes(config::CompoundConfigNode config);
    void AddArchMap(std::string name){ archTopo_map_[name] = levelAttri_;}
};

void ParseArchTopology(config::CompoundConfigNode config, ArchTopo& archTopo);
    
}

}