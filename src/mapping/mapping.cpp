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
    {Node::Scope, "Scope"},
    {Node::Trans, "Trans"}
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


// load tile information from config file
TileNode::TileNode(config::CompoundConfigNode config): Node(Node::Tile, config){
    
    std::string type_s = "temporal";
    config.lookupValue("type", type_s);
    tolower(type_s);
    if (type_s.find("temp") != std::string::npos) {
        type_ = Temporal;
    }
    else if (type_s.find("spatial") != std::string::npos) {
        type_ = Spatial;
    }
    else {
        TILEEXP_ERROR("Unknown Tile type" << type_s);
    }
    
    // add loop boundary
    std::unordered_map<std::string, std::pair<int, int> > loop_bounds;
    std::string buffer;
    if (config.lookupValue("factors", buffer)) {
        loop_bounds = ParseFactors(buffer); // parse loop bound {dim, [low, high]}
    }
    else {
        TILEEXP_ERROR("No factors");
    }

    // add permutation
    std::vector<std::string> iters;
    if (config.lookupValue("permutation", buffer)) {
        iters = ParsePermutations(buffer);
    }
    else {
        for (auto& kv: loop_bounds) iters.push_back(kv.first);
        TILEEXP_WARNING("No permutation specified. Infer instead."); // permutation from factors
    }
    
    TILEEXP_ASSERT(iters.size() == loop_bounds.size(), "permutation " << buffer << " & factor iter mismatch");

    ParseStorageLevel(config); // 找到当前的storage level，并存储在storage_level_name_和storage_level_中 // TBD

    if (config.exists("multicast")) { // not use
        config.lookupValue("multicast", multicast_enabled_);
    }

    unsigned split = iters.size();
    config.lookupValue("split", split); // used
    for (int i = (int)iters.size()-1; i >= 0; --i) {
        loopnests_.emplace_back();
        loop::TileFlow::Descriptor& loop = loopnests_.back();
        loop.name_ = iters[i];
        loop.dimension = problem::GetShape()->FactorizedDimensionNameToID.at(loop.name_); // 全局的ID
        loop.start = 0;
        loop.end = loop_bounds[iters[i]].first;
        loop.residual_end = loop_bounds[iters[i]].second;
        loop.stride = 1;
        loop.spacetime_dimension = type_s == "spatial"? 
        ((unsigned)i < split? spacetime::Dimension::SpaceX : spacetime::Dimension::SpaceY) 
        : spacetime::Dimension::Time; 
    }
    
    name_ += type_ == Temporal? "::Temporal" : "::Spatial"; 
}

OpNode::OpNode(config::CompoundConfigNode config): Node(Node::Op, config) {
    assert(config.lookupValue("name", op_name_));
    p_workload = p_workloads_->get_workload(op_name_);
    name_ += "::" + op_name_;
}

} // namespace TileExp

} // namespace mapping
