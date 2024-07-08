#pragma once

#include "./model/level.hpp"
#include "./model/engine.hpp"
#include "./model/network-legacy.hpp"

//


namespace model{

namespace TileExp{

class GraphNode{
    private: 
        std::string name;
        std::string class_name;
        std::shared_ptr<model::Level> node;
        // std::shared_ptr<LevelSpecs> Node;
        std::shared_ptr<LegacyNetwork> net_node; // inferred network config
        std::string type;

    public:
        // construct
        GraphNode(); // default constructor
        // GraphNode(model::Engine::Specs InputSpecs);
        GraphNode(std::string Name, std::string className, std::shared_ptr<model::Level> Node);
        GraphNode(std::string Name, std::string className, std::shared_ptr<model::Level> Node, 
                                    std::shared_ptr<LegacyNetwork> NetNode);
        
        
        // funcitonal function
        void PrintNode();
        void PrintNet();
        // std::shared_ptr<LevelSpecs> LookupNode();
        std::shared_ptr<model::Level> LookupNode();
        std::shared_ptr<LegacyNetwork> LookupNetNode();

        // basic function
        std::string GetName();
        std::shared_ptr<model::Level> GetNode();
        std::shared_ptr<LegacyNetwork> GetNetNode();
        std::string GetType();

};



// receive model::Topology::Specs as input to build topology graph
class Graph{
    private: 
        // int val;
        std::string root;
        model::Engine::Specs specs;
        std::unordered_map<std::string, std::shared_ptr<GraphNode>> GraphNodeList; // Node list for lookup value
        std::unordered_map<std::string, std::vector<std::string>> GraphList; // adjacent list for -- string-name and LevelSpecs--NodeConfig
        // std::unordered_map<std::string, std::vector<std::shared_ptr<model::Level>>> levels; // adjacent list for -- string-name and LevelSpecs--NodeConfig
        // std::unordered_map<std::string, std::vector<std::shared_ptr<LegacyNetwork::Specs>>> NetGraphList; // adjacent list for -- string-name and LevelSpecs--NodeConfig
        unsigned node_num;
        unsigned arith_num;

    public:

        // construct
        Graph() = default;
        ~Graph() = default;
        Graph(model::Engine::Specs& InputSpecs); // inintialize specs and build graph --> build GraphList

        // void BuildGraph(model::Topology::Specs& TpSpecs, std::string Root, unsigned LevelNum);
        void BuildGraph(model::Topology::Specs& TpSpecs, unsigned LevelNum);

        // functional function
        std::shared_ptr<LegacyNetwork> FindLegacyNetwork(model::Topology::Specs& TpSpecs, std::string Name);
        void AddVertex(std::string Name);
        bool AddEdge(std::string Name, std::string className, std::shared_ptr<model::Level> LevelNode, std::shared_ptr<LegacyNetwork> NetNode); // add edge for each memory level
        bool AddEdge(std::string Name, std::string className, std::shared_ptr<model::Level> LevelNode);
        void Print();

        // find values
        unsigned GetNodeCount();
        
        // look up values by name
        model::BufferLevel::Specs& LookupBufferSpecs(std::string Name);
        model::LegacyNetwork::Specs& LookupNetSpecs(std::string Name);
        model::ArithmeticUnits::Specs& LookupArithSpecs(std::string Name);
        model::LegacyNetwork::Stats& LookupNetStats(std::string Name);
        
        // unsigned LookupValue(std::string Name, std::string ValueName, unsigned Value);
        // int LookupValue(std::string Name, std::string ValueName, int Value);
        // std::string Graph::LookupNetValue(std::string Name, std::string ValueName, std::string Value);
        
        std::vector<std::string> FindDestNode(std::shared_ptr<GraphNode> PtrGraphNode);
        std::string trim(const std::string str);
        std::vector<std::string> parseName2Vec(const std::string name);
        std::vector<std::string> ParseName(std::string Name);
        
        // basic
        std::string GetRoot();
        unsigned NumCount();
        void BFS(std::string NodeName, std::unordered_set<std::string>& Visited);
        
        // test
        bool Test();

};


} // end namespace TileExp
} //end namespace model