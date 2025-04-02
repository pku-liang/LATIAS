#include <regex>

#include "./TileExp/model/interconnection.hpp"

namespace Hardware{

namespace InterConnection{

std::unordered_map<std::string, ConType> String2ConType = {
    {"HD", ConType::HD},
    {"FD", ConType::FD},
    {"UC", ConType::UC},
    {"BC", ConType::BC},
    {"Ring", ConType::Ring},
    {"Mesh", ConType::Mesh}
};

void InterCon::Print() const{
    std::cout << "****** Print InterConnection ******" << std::endl;
    for (const auto& pair : intercon_map_) {
        const std::string& source = pair.first;
        const std::list<std::string>& target = pair.second;

        for (const auto& val : target) {
            source2target intername_ = source + "2" + val;
            if (intercon_attri_map_.at(intername_).fanin_ != 0){
                std::cout << source << " --> " << val << ", Fan-in: " << intercon_attri_map_.at(intername_).fanin_ << std::endl;
            }
            else if (intercon_attri_map_.at(intername_).fanout_ != 0){
                std::cout << source << " --> " << val << ", Fan-out: " << intercon_attri_map_.at(intername_).fanout_ << std::endl;
            }
            else TILEEXP_ASSERT(0, "Fanout and Fanin equal to 0");
        }
    }
    std::cout << "****** End Print InterConnection ******" << std::endl;
}

void ParseInterConnection(config::CompoundConfigNode config, InterCon& intercon, Hardware::ArchTopology::ArchTopo& archTopo){
    assert(config.exists("connection"));

    auto config_ = config.lookup("connection");
    assert(config_.isList());
    for (int i = 0; i < config_.getLength(); i++){
        auto config_node = config_[i];
        std::string source, target;
        config_node.lookupValue("source", source);
        config_node.lookupValue("target", target);
        unsigned int read_bandwidth, write_bandwidth;
        config_node.lookupValue("read_bandwidth", read_bandwidth);
        config_node.lookupValue("write_bandwidth", write_bandwidth);
        std::string con_type;
        config_node.lookupValue("type", con_type);
        auto type = String2ConType.find(con_type);
        assert(type != String2ConType.end());
        
        InterConNode node(target, source, read_bandwidth, write_bandwidth, String2ConType[con_type]);
        auto fan_target_ = archTopo.archTopo_map_[target].level_num;
        auto fan_source_ = archTopo.archTopo_map_[source].level_num;
        if (fan_target_ >= fan_source_) node.fanout_ = fan_target_ / fan_source_;
        else node.fanin_ = fan_source_ / fan_target_;

        intercon.intercon_map_[source].push_back(target);
        intercon.intercon_attri_map_[source + "2" + target] = node;
    }
}

}

}