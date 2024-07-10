#pragma once 

#include "model/engine.hpp"
#include "compound-config/compound-config.hpp"
#include "mapping/mapping.hpp"

#include "TileExp/common.hpp"
#include "TileExp/problem/problem.hpp"


namespace mapping{

namespace TileExp{

const std::unordered_map<Node::type_t, std::string> Node::type2name_ = {
    {Node::Tile, "Tile"},
    {Node::Op, "Op"},
    {Node::Scope, "Scope"}
};

cosnt std::unordered_map<std::string, Node::type_t> name2dataflow_mode_ = {
    {"forward", Forward},
    {"write-back", Write_back}
};

Node::Node(
    Node::type_t t, 
    config::CompoundConfigNode config): type_(t){
    name_ = type2name_.at(type_);

    if (config.exists("bypass"))
        config.lookupArrayValue("bypass", bypassed_);
    
    // Ray -- add dataflow mode
    std::string dataflow_mode_s;
    config.lookupValue("dataflow-mode", dataflow_mode_s)
    dataflow_mode_ = name2dataflow_mode_.at(dataflow_mode_s);

    config.lookupValue("profile", profile_);
    std::string tag = "";
    config.lookupValue("tag", tag);
    if (tag.size())
        name_ += "::" + tag;
}

ScopeNode::ScopeNode(config::CompoundConfigNode config): Node(Node::Scope, config){
    std::string type_s = "sequential";
    config.lookupValue("type", type_s);
    tolower(type_s);
    if (type_s.find("seq") != std::string::npos) {
        type = Sequential;
        name_ += "::Sequential";
    }
    else if (type_s.find("para") != std::string::npos) {
        type = Parallel;
        name_ += "::Parallel";
    }
    else if (type_s.find("pipe") != std::string::npos) {
        type = Pipeline;
        name_ += "::Pipeline";
    }
    else if (type_s.find("shar") != std::string::npos) {
        type = Sharing;
        name_ += "::Sharing";
    }
    else {TILEEXP_ERROR("ScopeNode type error. Should has type sequential/parallel");}

}   

TileNode::TileNode(config::CompoundConfigNode config){
    
}

OpNode::OpNode(config::CompoundConfigNode config): Node(Node::Op, config) {
    assert(config.lookupValue("name", op_name_));
    p_workload = p_workloads_->get_workload(op_name_);
    name_ += "::" + op_name_;
}

} // namespace TileExp

} // namespace mapping
