#include <regex>

#include "mapping/arch-properties.hpp"

#include "TileExp/mapping/mapping.hpp"
#include "TileExp/mapper/expr.hpp"
// #include "TileExp/model/graph.hpp"

using TileExp::macros;
using TileExp::verbose_level;

namespace mapping{

namespace TileExp{

// const problem::TileExp::Workloads* p_workloads_ = nullptr;

Node* RecursiveParse(config::CompoundConfigNode config);

ExpMapping ParseAndConstruct(config::CompoundConfigNode config,
                          model::TileExp::Graph& graph,
                          const problem::TileExp::Workloads& workloads, 
                          model::Engine::Specs& arch_specs)
{
    // arch_props_ = ArchProperties();
    // arch_props_.Construct(arch_specs);
    // arch_specs_ = arch_specs;
    // auto arch_specs_ = arch_specs;

    p_workloads_ = &workloads;

    auto graph_ = graph;

    ExpMapping mapping;
    // build mapping tree
    mapping.root = RecursiveParse(config); //
    mapping.arch_specs_ = arch_specs;
    // build fanout map -- graph-based
    mapping.ParseFanoutMap(graph); // TBD

    mapping.PrintFanoutMap();

    return mapping;
}

MeshXYPair ExpMapping::GetNodeMeshXY(std::shared_ptr<model::TileExp::GraphNode> ptr_node_,
                                     std::string type_){
    // auto type_ = mapping::LevelName2TypeMap.at(name_);
    // std::shared_ptr<GraphNode> ptr_graph_node = GraphNodeList[Name];
    std::shared_ptr<model::Level> ptr_level_node = ptr_node_->GetNode();
    if (type_ == "BufferLevel"){
        auto ptr_buffer_node = std::static_pointer_cast<model::BufferLevel>(ptr_level_node);
        model::BufferLevel::Specs& buffer_specs = ptr_buffer_node->GetSpecs(); 
        if (buffer_specs.technology.Get() == model::BufferLevel::Technology::DRAM){
            return MeshXYPair(0.0, 0.0); // for DRAM
        }
        return MeshXYPair(float(buffer_specs.meshX.Get()), float(buffer_specs.meshY.Get()));
    }
    else if (type_ == "ArithmeticUnits")
    {
        auto ptr_arith_node = std::static_pointer_cast<model::ArithmeticUnits>(ptr_level_node);
        model::ArithmeticUnits::Specs& arith_specs = ptr_arith_node->GetSpecs(); 
        return MeshXYPair(float(arith_specs.meshX.Get()), float(arith_specs.meshY.Get()));
    }
    else{
        TILEEXP_ERROR("Error: Unrecognized node type: " + type_);
    }
    
    // return MeshXYPair(0, 0);
}

bool CheckFanoutLegal(MeshXYPair current_meshXY, MeshXYPair next_meshXY){
    if (unsigned(current_meshXY.first) % unsigned(next_meshXY.first) == 0 ||    
        unsigned(next_meshXY.first) % unsigned(current_meshXY.first) == 0){}
    else return false;

    if (unsigned(current_meshXY.second) % unsigned(next_meshXY.second) == 0 ||    
        unsigned(next_meshXY.second) % unsigned(current_meshXY.second) == 0){}
    else return false;

    if (next_meshXY.first / current_meshXY.first <= 1 && 
        next_meshXY.second / current_meshXY.second <= 1){}
    else if (next_meshXY.first / current_meshXY.first >= 1 && 
             next_meshXY.second / current_meshXY.second >= 1){}
    else return false;

    return true;
}

void ExpMapping::ParseFanoutMap(model::TileExp::Graph& graph){
    // TBD
    auto graph_ = graph;
    // auto current_node_ = graph.GetRoot();
    auto graph_list_ = graph.GetGraphList();
    auto graph_node_list_ = graph.GetGraphNodeList();

    for (auto& node_pair_: graph_list_){
        std::string current_name_ = node_pair_.first;
        auto current_node_ = graph_node_list_.at(current_name_);
        auto current_type_ = current_node_->GetType();
        MeshXYPair current_meshXY_ = GetNodeMeshXY(current_node_, current_type_);
        if (current_meshXY_ == MeshXYPair(0.0, 0.0)){
            current_meshXY_ = MeshXYPair(1.0, 1.0); // for DRAM
        }
        // auto x = current_meshXY_.first;
        // if(x){};
        TargetMeshXYMap TargetMeshXYMap_;
        for (std::string& next_name_: node_pair_.second){
            auto next_node_ = graph_node_list_.at(next_name_);
            auto next_type_ = next_node_->GetType();
            MeshXYPair next_meshXY_ = GetNodeMeshXY(next_node_, next_type_);
            if (next_meshXY_ == MeshXYPair(0.0, 0.0)){
                next_meshXY_ = current_meshXY_; // for DRAM
            }

            // bool in_out_X_ = next_meshXY_.first / current_meshXY_.first < 1; 
            TILEEXP_ASSERT(CheckFanoutLegal(current_meshXY_, next_meshXY_), 
                        "Error: Fanout of " + current_name_ + " to " + next_name_ + " is illegal");
            MeshXYPair FanoutXY = MeshXYPair(next_meshXY_.first / current_meshXY_.first, 
                                             next_meshXY_.second / current_meshXY_.second);
            TargetMeshXYMap_[next_name_] = FanoutXY;
            // std::cout << "current_node_ = " << current_name_ << " next_node_ = " << next_name_ << std::endl;
        }
        ExpFanoutXYMap_[current_name_] = TargetMeshXYMap_;
    }
        // auto level_ = mapping::LevelName2IdxMap.at(current_node_);

}

Node* RecursiveParse(config::CompoundConfigNode config){
    std::string node_type; 

    if (!config.lookupValue("node-type", node_type)) {
        TILEEXP_ERROR("No node-type is specified.");
    }
    tolower(node_type);

    Node * node = nullptr;
    if (node_type == "tile") {
        node = new TileNode(config);
    }
    else if (node_type == "op") {
        node = new OpNode(config);
    }
    else if (node_type == "scope") {
        node = new ScopeNode(config);
    }
    else if (node_type == "trans"){
        node = new TransNode(config); // TBD
    }
    else {
        TILEEXP_ERROR("Current node type is not a valid type.");
    }
        
    assert(node != nullptr);
    
    if (config.exists("subtree")) {
        config = config.lookup("subtree");
        if (config.isList()){
            for (int i = 0; i < config.getLength(); i++){
                node->add_child(RecursiveParse(config[i])); // TBD add_child
            }
        }
        else {
            node->add_child(RecursiveParse(config));
        }
    }
    // graph mode can tolerate non-Op leaf node
    // else if (node_type != "op") {
    //     TILEFLOW_ERROR("Exist non-Op leaf node.");
    // }

    return node;
}


std::unordered_map<std::string, std::pair<int, int> > Node::ParseFactors(
    const std::string& buffer) {
    std::unordered_map<std::string, std::pair<int, int> > loop_bounds;
    std::regex re("([A-Za-z]+)[[:space:]]*[=]*[[:space:]]*([0-9A-Za-z_?]+)(,([0-9]+))?", std::regex::extended);
    std::smatch sm;
    std::string str = std::string(buffer);
    str = str.substr(0, str.find("#")); // remove comments

    while (std::regex_search(str, sm, re)) // each time load one []=[]
    {
        std::string dimension_name = sm[1];

        int end;
        if (macros.exists(sm[2])){
            macros.lookupValue(sm[2], end);
        }
        else {
            char* ptr = nullptr;
            std::string var = sm[2];
            if (var.find("X") != std::string::npos || var.find("?") != std::string::npos){
                end = Symbol::global_symbol_table_.insert(sm[2]);
            }
            else{
                end = std::strtol(sm[2].str().c_str(), &ptr, 10); // return var -idx
            }
            // if (ptr && *ptr) {
            //     end = TileExp::global_symbol_table_.insert(sm[2]);
            // }
        }

        int residual_end = end;
        if (sm[4] != "")
        {
            // char* ptr = nullptr;
            std::string var = sm[4];
            if (var.find("X") != std::string::npos || var.find("?") != std::string::npos){
                residual_end = Symbol::global_symbol_table_.insert(sm[4]);
            }
            else{
                // residual_end = std::strtol(sm[4].str().c_str(), &ptr, 10); // return var -idx
                residual_end = std::stoi(sm[4]);
            }
        }

        loop_bounds[dimension_name] = {end, residual_end};

        str = sm.suffix().str();
    }

    return loop_bounds;
}

std::vector<std::string> Node::ParsePermutations(
    const std::string & buffer 
){
    std::vector<std::string> iters;
    
    std::istringstream iss(buffer);
    char token;
    while (iss >> token) {
        iters.push_back(std::string(1, token));
    }
    
    return iters;
}

void Node::ParseStorageLevel(config::CompoundConfigNode directive, model::Engine::Specs arch_specs)
{
    auto topology = arch_specs.topology;
    auto num_arith_levels = topology.ArithmeticMap() + 1;
    // auto num_storage_levels = topology.NumLevels() - num_arith_levels;
        
    //
    // Find the target storage level. This can be specified as either a name or an ID.
    //
    std::string storage_level_name;
    unsigned storage_level_id;
        
    if (directive.lookupValue("target", storage_level_name)) // find current memory level id
    {
        // Find this name within the storage hierarchy in the arch specs.
        for (storage_level_id = num_arith_levels; storage_level_id < topology.NumLevels(); storage_level_id++)
        {
        if (topology.GetLevel(storage_level_id)->level_name == storage_level_name)
            break;
        }
        if (storage_level_id == topology.NumLevels())
        {
        std::cerr << "ERROR: target storage level not found: " << storage_level_name << std::endl;
        exit(1);
        }
    }
    else
    {
        int id;
        assert(directive.lookupValue("target", id));
        assert(id >= int(num_arith_levels)  && id < int(topology.NumLevels()));
        storage_level_id = static_cast<unsigned>(id);
    }

    assert(storage_level_id < topology.NumLevels());

    storage_level_name_ = storage_level_name;
    storage_level_ = storage_level_id;
    name_ += "::" + storage_level_name_;
}

} // namespace TileExp

} // namespace mapping
