#include "./TileExp/model/graph.hpp"
#include "./model/topology.hpp"
#include "./TileExp/common.hpp"

namespace model{

namespace TileExp{

// ******* GraphNode ******** //
// construct memory
GraphNode::GraphNode(std::string Name, std::string className, std::shared_ptr<model::Level> Node, 
                                    std::shared_ptr<LegacyNetwork> NetNode){
    name = Name;
    node = Node;
    class_name = className;
    net_node = NetNode;
    if (model::isComputeClass(className)) type = "ArithmeticUnits";
    else if (model::isBufferClass(className)) type = "BufferLevel";
    else assert(false);
}

// arith
GraphNode::GraphNode(std::string Name, std::string className, std::shared_ptr<model::Level> Node){
    name = Name;
    node = Node;
    class_name = className;
    net_node = nullptr;
    if (model::isComputeClass(className)) type = "ArithmeticUnits";
    else if (model::isBufferClass(className)) type = "BufferLevel";
    else assert(false);
}

std::string GraphNode::GetName(){
    return name;
}

std::shared_ptr<model::Level> GraphNode::GetNode(){
    return node;
}

std::shared_ptr<LegacyNetwork> GraphNode::GetNetNode(){
    return net_node;
}

std::string GraphNode::GetType(){
    return type;
}

// ******* End GraphNode **** //

// ******* Graph ******** //
// construct 
Graph::Graph(model::Engine::Specs& InputSpecs){
    specs = InputSpecs;
    unsigned level_num = specs.topology.NumLevels();
    auto& topology = specs.topology;
    root = topology.GetLevel(level_num-1)->level_name; // name-->root
    node_num = level_num;
    arith_num = topology.ArithmeticMap() + 1;

    std::cout << "start build graph" << std::endl;
    BuildGraph(topology, level_num);
    // BuildGraph(topology, root, level_num);
    
}

// construct graph (adjacent list) by model::Topology::Specs
void Graph::BuildGraph(model::Topology::Specs& TpSpecs, unsigned LevelNum){
// void Graph::BuildGraph(model::Topology::Specs& TpSpecs, std::string Root, unsigned LevelNum){
    for(int i = LevelNum - 1; i > -1; i--){

        // need to be casted to different class
        auto tmp_specs = TpSpecs.GetLevel(i);
        auto className = tmp_specs->className;
        auto name = tmp_specs->level_name;

        std::cout << "Node Name: " << name << std::endl;

        if (tmp_specs->Type() == "ArithmeticUnits"){ // without inferred network
        // if (model::isComputeClass(className)){ // without inferred network
            // level -- deep copy
            model::ArithmeticUnits::Specs& current_specs = *std::static_pointer_cast<model::ArithmeticUnits::Specs>(tmp_specs);
            std::shared_ptr<model::ArithmeticUnits> arithmetic_level = std::make_shared<model::ArithmeticUnits>(current_specs);
            std::shared_ptr<model::Level> level = std::static_pointer_cast<model::Level>(arithmetic_level);

            // add edge
            assert(AddEdge(name, className, level));
        }
        else if (tmp_specs->Type() == "BufferLevel"){ // with inferred network
        // else if (model::isBufferClass(className)){ // with inferred network
            // level
            model::BufferLevel::Specs& current_specs = *std::static_pointer_cast<BufferLevel::Specs>(tmp_specs);
            std::shared_ptr<BufferLevel> buffer_level = std::make_shared<BufferLevel>(current_specs);
            std::shared_ptr<model::Level> level = std::static_pointer_cast<Level>(buffer_level);

            // legacy network
            std::shared_ptr<LegacyNetwork> legacy_net = FindLegacyNetwork(TpSpecs, name);

            // add edge
            assert(AddEdge(name, className, level, legacy_net));
        }
        else{
            TILEEXP_ERROR("Not supported node type" + name);
        }
    }
}

// functional function
std::shared_ptr<LegacyNetwork> Graph::FindLegacyNetwork(model::Topology::Specs& TpSpecs, std::string Name){
    for (unsigned i = 0; i < TpSpecs.NumStorageLevels(); i++){
        auto legacy_net = *TpSpecs.GetInferredNetwork(i);
        if (legacy_net.name.find(Name) != std::string::npos){
            // std::cout << "legacy net name: " << legacy_net.name << std::endl;
            // std::cout << "Name: " << Name << std::endl;         
            std::shared_ptr<LegacyNetwork> legacy_network = std::make_shared<LegacyNetwork>(legacy_net);
            return legacy_network;
        }
    }
    assert(false);
}

// template<typename T>
// init 
void Graph::AddVertex(std::string Name){
    if (GraphList.find(Name) == GraphList.end()){
        GraphList[Name] = std::vector<std::string>();
    }
}

std::string Graph::trim(const std::string str) {
    
    size_t first_s = str.find_first_not_of(' ');
    size_t last_s = str.find_last_not_of(' ');

    auto str_tmp = str.substr(first_s, last_s - first_s + 1);

    size_t first_l = str_tmp.find_first_not_of('[');
    size_t last_r = str_tmp.find_last_not_of(']');

    return str_tmp.substr(first_l, last_r - first_l + 1);
}

std::vector<std::string> Graph::parseName2Vec(const std::string name) {
    std::vector<std::string> names;
    size_t start = 0;
    size_t end = name.find(',');

    while (end != std::string::npos) {
        std::string part = trim(name.substr(start, end - start));
        if (!part.empty()) {
            names.push_back(part);
        }
        start = end + 1;
        end = name.find(',', start);
    }

    std::string part = trim(name.substr(start));
    if (!part.empty()) {
        names.push_back(part);
    }

    return names;
}

std::vector<std::string> Graph::ParseName(std::string Name){
    std::vector<std::string> result;
    auto pos_start = Name.find("[");
    auto pos_dot = Name.find(",");
    Name = Name.substr(pos_start);
    while (pos_dot != std::string::npos){
        result.push_back(Name.substr(0, pos_dot));
        Name = Name.substr(pos_dot);
        pos_dot = Name.find(",");
    }
    // last ,
    auto pos_end = Name.find("]");
    result.push_back(Name.substr(0, pos_end));
    return result;
} 

// find successor nodes
std::vector<std::string> Graph::FindDestNode(std::shared_ptr<GraphNode> PtrGraphNode){
    std::vector<std::string> result;
    if (PtrGraphNode->GetType() == "ArithmeticUnits"){
        std::shared_ptr<model::ArithmeticUnits> current_units = std::static_pointer_cast<model::ArithmeticUnits>(PtrGraphNode->GetNode());
        auto current_specs = current_units->GetSpecs();
        std::string successor = current_specs.successor.Get();
        result = parseName2Vec(successor);
        // result = current_specs.successor.Get();
    }
    else if (PtrGraphNode->GetType() == "BufferLevel"){
        std::shared_ptr<model::BufferLevel> current_units = std::static_pointer_cast<model::BufferLevel>(PtrGraphNode->GetNode());
        auto current_specs = current_units->GetSpecs();
        std::string successor = current_specs.successor.Get();
        result = parseName2Vec(successor);
        // result = current_specs.successor.Get();
    }
    else assert(false);
    return result;
}

// for memory units
bool Graph::AddEdge(std::string Name, std::string className, std::shared_ptr<model::Level> LevelNode, std::shared_ptr<LegacyNetwork> NetNode){
    // build Node
    // std::shared_ptr<GraphNode> ptr_graph_node = std::make_shared<GraphNode>(GraphNode(Name, LevelNode, NetNode)); // add more, 需要展示出Name和type的关系
    std::shared_ptr<GraphNode> ptr_graph_node(new GraphNode(Name, className, LevelNode, NetNode)); // add more, 需要展示出Name和type的关系
    std::vector<std::string> dest_node = FindDestNode(ptr_graph_node); // add more

    // add edge
    AddVertex(Name); // src
    
    GraphNodeList[Name] = ptr_graph_node; // store node
    for (auto& node: dest_node){
        AddVertex(node); // dest
        GraphList[Name].push_back(node);
    }

    return true;
}

// for compute units
bool Graph::AddEdge(std::string Name, std::string className, std::shared_ptr<model::Level> LevelNode){
    // build Node
    // std::shared_ptr<GraphNode> ptr_graph_node = std::make_shared<GraphNode>(GraphNode(Name, LevelNode, NetNode)); // add more, 需要展示出Name和type的关系
    std::shared_ptr<GraphNode> ptr_graph_node(new GraphNode(Name, className, LevelNode)); // add more -- finish, 需要展示出Name和type的关系
    std::vector<std::string> dest_node = FindDestNode(ptr_graph_node); // add more -- finish

    // add edge
    AddVertex(Name); // src
    
    GraphNodeList[Name] = ptr_graph_node; // store node
    for (auto& node: dest_node){
        AddVertex(node); // dest
        GraphList[Name].push_back(node);
    }

    return true;
}

void Graph::Print(){
    std::cout << "========== Graph start ==========" << std::endl;
    std::cout << "Arch Root: " << root << std::endl;
    std::cout << "Arch Node Num: " << node_num << std::endl;
    // print graph node as our paper said
    auto current_node = root;
    std::unordered_set<std::string> visited;
    BFS(current_node, visited);
    std::cout << "=========== Graph end ===========" << std::endl;
}

void Graph::BFS(std::string NodeName, std::unordered_set<std::string>& Visited){
    if(Visited.count(NodeName)) return;

    std::cout << "Node:" << NodeName << std::endl;
    Visited.insert(NodeName);

    std::vector<std::string> wait_list;

    for(auto &dest: GraphList[NodeName]){
        std::cout << "Edge:" << NodeName << " to " << dest <<  std::endl;
        wait_list.push_back(dest);
    }

    for(auto& dest: wait_list){
        BFS(dest, Visited);
    }
}

// find values
unsigned Graph::GetNodeCount(){
    return GraphNodeList.size();
}

model::BufferLevel::Specs& Graph::LookupBufferSpecs(std::string Name){
    // std::cout << "LookupValue: " << Name << " " << ValueName << " " << Value << std::endl;
    std::shared_ptr<GraphNode> ptr_graph_node = GraphNodeList[Name];
    std::shared_ptr<model::Level> ptr_level_node = ptr_graph_node->GetNode();
    if (ptr_graph_node->GetType() == "BufferLevel"){
        auto ptr_buffer_node = std::static_pointer_cast<model::BufferLevel>(ptr_level_node);
        model::BufferLevel::Specs& buffer_specs = ptr_buffer_node->GetSpecs(); 
        return buffer_specs;
    }
    else assert(false);
}

model::LegacyNetwork::Specs& Graph::LookupNetSpecs(std::string Name){
    std::shared_ptr<GraphNode> ptr_graph_node = GraphNodeList[Name];
    std::shared_ptr<model::LegacyNetwork> ptr_net_node = ptr_graph_node->GetNetNode();
    if (ptr_graph_node->GetType() == "BufferLevel"){
        model::LegacyNetwork::Specs& net_specs = ptr_net_node->GetSpecs(); 
        return net_specs;
    }
    else assert(false);
}

model::LegacyNetwork::Stats& Graph::LookupNetStats(std::string Name){
    std::shared_ptr<GraphNode> ptr_graph_node = GraphNodeList[Name];
    std::shared_ptr<model::LegacyNetwork> ptr_net_node = ptr_graph_node->GetNetNode();
    if (ptr_graph_node->GetType() == "BufferLevel"){
        model::LegacyNetwork::Stats& net_stats = ptr_net_node->stats_; 
        return net_stats;
    }
    else assert(false);
}

model::ArithmeticUnits::Specs& Graph::LookupArithSpecs(std::string Name){
    std::shared_ptr<GraphNode> ptr_graph_node = GraphNodeList[Name];
    std::shared_ptr<model::Level> ptr_level_node = ptr_graph_node->GetNode();
    if (ptr_graph_node->GetType() == "ArithmeticUnits"){
        auto ptr_arith_node = std::static_pointer_cast<model::ArithmeticUnits>(ptr_level_node);
        model::ArithmeticUnits::Specs& arith_specs = ptr_arith_node->GetSpecs(); 
        return arith_specs;
    }
    else assert(false);
}

// basic function
std::string Graph::GetRoot(){
    return root;
}

// test function
bool Graph::Test(){
    Print();
    return true;
}


// ******* End Graph ******** //

} // end namespace TileExp
} //end namespace model