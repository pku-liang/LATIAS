#include "model/engine.hpp"
#include "compound-config/compound-config.hpp"
#include "mapping/mapping.hpp"

#include "TileExp/common.hpp"
#include "TileExp/problem/problem.hpp"
#include "TileExp/mapping/mapping.hpp"
#include "TileExp/mapping/parser.hpp"


namespace mapping{

namespace TileExp{

// void tolower(std::string& str){
//     std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {return std::tolower(c);});
// }

const std::unordered_map<Node::type_t, std::string> Node::type2name_ = {
    {Node::Tile, "Tile"},
    {Node::Op, "Op"},
    {Node::Scope, "Scope"},
    {Node::Trans, "Trans"}
};

const std::unordered_map<std::string, dataflow_mode> name2dataflow_mode_ = {
    {"forward", dataflow_mode::Forward},
    {"write-back", dataflow_mode::Write_back}
};

Node::Node(
    Node::type_t t, 
    config::CompoundConfigNode config): type_(t){
    name_ = type2name_.at(type_);

    // Ray -- add target level
    if (config.exists("target")){
        config.lookupValue("target", target_level_name);
        if (mapping::LevelName2IdxMap.find(target_level_name) == mapping::LevelName2IdxMap.end()) {
            TILEEXP_ERROR("Target level " << target_level_name << " is not defined.");
        }
        target_level_id = mapping::LevelName2IdxMap.at(target_level_name);
    }
    else{
        TILEEXP_ERROR("No target level is specified.");
    }

    if (config.exists("bypass")) config.lookupArrayValue("bypass", bypassed_);
    
    // Ray -- add dataflow mode
    std::string dataflow_mode_s;
    config.lookupValue("dataflow-mode", dataflow_mode_s);
    dataflow_mode_ = name2dataflow_mode_.at(dataflow_mode_s);

    // may error
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

    // this part now in Node()
    // ParseStorageLevel(config); // 找到当前的storage level，并存储在storage_level_name_和storage_level_中

    if (config.exists("multicast")) { 
        config.lookupValue("multicast", multicast_enabled_);
    }

    // TBD
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

loop::Nest TileNode::constructLoopNest(const SymbolTable* symbol_table_) const{
    loop::Nest loop_nest;
    uint64_t num_subnests_added = 0;
    for (auto loop: loopnests_)
    {
        // Ignore trivial factors
        // This reduces computation time by 1.5x on average.
        if (loop.end <= 0) {
            assert(symbol_table_);
            loop.residual_end = loop.end = symbol_table_->lookup(loop.end).value_;
        }
        if (loop.start + loop.stride < loop.end){
            assert((type_==TileNode::Spatial && loop::IsSpatial(loop.spacetime_dimension))
            || (type_==TileNode::Temporal && !loop::IsSpatial(loop.spacetime_dimension)));
            loop_nest.AddLoop(loop);
            num_subnests_added ++;
        }
    }
    if (num_subnests_added == 0) {
        loop_nest.AddLoop(0, 0, 1, 1, type_ == TileNode::Spatial? spacetime::Dimension::SpaceX : spacetime::Dimension::Time);
    }
    loop_nest.AddStorageTilingBoundary();
    return loop_nest;
}


OpNode::OpNode(config::CompoundConfigNode config): Node(Node::Op, config) {
    assert(config.lookupValue("name", op_name_));
    // p_workload = p_workloads_->get_workload(op_name_); // TBD
    name_ += "::" + op_name_;
}

TransNode::TransNode(config::CompoundConfigNode config): Node(Node::Trans, config) {
    // assert(config.lookupValue("name", trans_name_));
    // name_ += "::" + trans_name_;
}


void Node::add_child(const Node* child){
    // if (type_ == Node::Scope) {
        
    //     unsigned storage_level;
    //     std::string storage_level_name = "Unknown";
    //     if (child->get_type() == Node::Tile) {
    //         // if (static_cast<const TileNode*>(child)->get_tile_type() == TileNode::Temporal){
    //         //     storage_level = child->get_storage_level() + 1;
    //         // }
    //         // else {
    //             storage_level = child->get_storage_level();
    //             storage_level_name = child->get_storage_name();
    //         // }
    //     }
    //     else if (child->get_type() == Node::Scope) {
    //         storage_level = child->get_storage_level();
    //         storage_level_name = child->get_storage_name();
    //     }
    //     else {
    //         TILEFLOW_ERROR("Scope Node should not have a op child");
    //     }
    //     assert(storage_level_ == unsigned(-1) || storage_level_ == storage_level);
    //     storage_level_ = storage_level;
    //     storage_level_name_ = storage_level_name;
    // }
    assert(child != nullptr); 
    children_.push_back(child); 
    child->set_parent(this);
}

} // namespace TileExp

} // namespace mapping
