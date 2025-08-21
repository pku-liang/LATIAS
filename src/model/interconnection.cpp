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
            auto format_source = format_fixed(source, 12);
            auto format_target = format_fixed(val, 12);
            auto fan_in_ = intercon_attri_map_.at(intername_).fanin_;
            auto fan_out_ = intercon_attri_map_.at(intername_).fanout_;
            auto format_fan_in = format_fixed(std::to_string(fan_in_), 4);
            auto format_fan_out = format_fixed(std::to_string(fan_out_), 4);

            if (intername_ == "Ring2Ring"){
                continue;
            }
            else if (intername_ == "Mesh2Mesh"){
                continue;
            }

            if (fan_in_ != 0 && fan_in_ != fan_out_){
                std::cout << format_source << " --> " << format_target << ", Fan-in: " << format_fan_in << std::endl;
            }
            else if (fan_out_ != 0 && fan_in_ != fan_out_) {
                std::cout << format_source << " --> " << format_target << ", Fan-out: " << format_fan_out << std::endl;
            }
            else if (fan_in_ == fan_out_) std::cout << format_source << " --> " << format_target << ", Fan-in-out: " << format_fan_out << std::endl;
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
        unsigned int bandwidth;
        config_node.lookupValue("source", source);
        config_node.lookupValue("target", target);
        config_node.lookupValue("bandwidth", bandwidth);
        // unsigned int read_bandwidth, write_bandwidth;
        // config_node.lookupValue("read_bandwidth", read_bandwidth);
        // config_node.lookupValue("write_bandwidth", write_bandwidth);
        std::string con_type;
        config_node.lookupValue("type", con_type);
        auto type = String2ConType.find(con_type);
        assert(type != String2ConType.end());
        
        if (target == "Inner"){
            target = con_type;
        }

        InterConNode node(target, source, bandwidth, String2ConType[con_type]);
        unsigned int fan_target_, fan_source_;
        if (target == con_type){
            fan_target_ = 1;
            fan_source_ = 1;
        }
        else{
            fan_target_ = archTopo.archTopo_map_[target].level_num;
            fan_source_ = archTopo.archTopo_map_[source].level_num;
        }
        if (fan_target_ > fan_source_) node.fanout_ = fan_target_ / fan_source_;
        else if (fan_target_ == fan_source_) node.fanout_ = node.fanin_ = 1;
        else node.fanin_ = fan_source_ / fan_target_;

        intercon.intercon_map_[source].push_back(target);
        intercon.intercon_attri_map_[source + "2" + target] = node;
    }
}

}

}