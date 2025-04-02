#pragma once

#include "mapping/parser.hpp"

#include "./TileExp/common.hpp"
#include "./TileExp/model/hardware.hpp"

typedef std::string source2target;

namespace Hardware{

namespace InterConnection{
    
enum ConType{
    HD, // half-duplex
    FD, // full duplex
    UC, // unicast
    BC, // broadcast
    Ring, // NoC
    Mesh, // NoC
    UNKNOWN
};

extern std::unordered_map<std::string, ConType> con_type_map;

class InterConNode{
public:
    std::string target_;
    std::string source_;
    unsigned int read_bandwidth_;
    unsigned int write_bandwidth_;
    ConType con_type_;

    unsigned int fanin_ = 0;
    unsigned int fanout_ = 0;

    InterConNode(){};
    InterConNode(std::string target, std::string source, unsigned int read_bandwidth, unsigned int write_bandwidth, ConType con_type): target_(target), source_(source), read_bandwidth_(read_bandwidth), write_bandwidth_(write_bandwidth), con_type_(con_type){};
};

class InterCon{
public:
    // config::CompoundConfigNode config_;
    InterConNode intercon_;
    std::unordered_map<std::string, std::list<std::string>> intercon_map_;
    std::unordered_map<source2target, InterConNode> intercon_attri_map_;
    // InterCon(config::CompoundConfigNode config);
    void Print() const;
};

void ParseInterConnection(config::CompoundConfigNode config, InterCon& intercon, Hardware::ArchTopology::ArchTopo& archTopo);

}
}