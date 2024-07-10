#include <regex>

#include "mapping/arch-properties.hpp"

#include "TileExp/mapping/mapping.hpp"
// #include "TileExp/mapper/mapper.hpp"

using TileFlow::macros;
using TileFlow::verbose_level;

namespace mapping{

namespace TileExp{


ExpMapping ParseAndConstruct(config::CompoundConfigNode config,
                          model::TileExp::Graph& graph,
                          const problem::TileFlow::Workloads& workloads)
{
    // arch_props_ = ArchProperties();
    // arch_props_.Construct(arch_specs);
    p_workloads_ = &workloads;

    ExpMapping mapping;
    mapping.root = RecursiveParse(config); // TBD
    mapping.ParseFanoutMap(graph); // TBD

    return mapping;
}

Node* RecursiveParse(config::CompoundConfigNode config){
    std::string node_type; 

    if (!config.lookupValue("node-type", node_type)) {
        TILEEXP_ERROR("No node-type is specified.");
    }
    tolower(node_type);

    Node * node = nullptr;
    if (node_type == "tile") {
        node = new TileNode(config); // TBD
    }
    else if (node_type == "op") {
        node = new OpNode(config); // TBD
    }
    else if (node_type == "scope") {
        node = new ScopeNode(config); // TBD
    }
    else {
        TILEFLOW_ERROR(node_type << " is not a valid type.");
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

} // namespace TileExp

} // namespace mapping
