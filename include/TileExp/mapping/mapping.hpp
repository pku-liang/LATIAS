#pragma once 

#include "model/engine.hpp"
#include "compound-config/compound-config.hpp"
#include "mapping/mapping.hpp"

#include "TileExp/common.hpp"
#include "TileExp/problem/problem.hpp"


namespace mapping{

namespace TileExp{

typedef std::unordered_map<std::string, std::string> StringMap;
const problem::TileExp::Workloads* p_workloads_ = nullptr;

enum dataflow_mode{
    Forward,
    Write_back
};

class Node {
public: 
    enum type_t{
        Tile,
        Op,
        Scope,
        Trans
    };
protected:
    static const std::unordered_map<type_t, std::string> type2name_; 
    Node::type_t type_;
    std::string name_;
    mutable const Node* parent_ = nullptr;
    std::vector<const Node*> children_;
    mutable std::string storage_level_name_;
    mutable unsigned storage_level_ = unsigned(-1);
    std::vector<std::string> bypassed_; // decide whether to bypass this node
    bool profile_ = true;

    dataflow_mode dataflow_mode_;

    mutable ActiveTensor active_tensors_;

public:
    Node(type_t, config::CompoundConfigNode config);
    std::unordered_map<std::string, std::pair<int, int> > ParseFactors(const std::string& buffer);
    std::vector<std::string> ParsePermutations(const std::string & buffer);
    Node::ParseStorageLevel(config::CompoundConfigNode directive); 

    virtual void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const; // TBD

}

class TileNode: public Node {
public:
    enum type_t {
        Temporal,
        Spatial
    };
private:

    // std::vector<loop::TileFlow::Descriptor> loopnests_; // 描述当前循环 -- 暂时注释
    TileNode::type_t type_;
    bool multicast_enabled_ = true;

public:
    TileNode(config::CompoundConfigNode config);
    void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    // void accept(Visitor* visitor) const {visitor->visitTile(this);}
    bool is_spatial() const {return type_ == Spatial;}
    bool is_multicast_enabled() const {return multicast_enabled_;}
    TileNode::type_t get_tile_type() const {return type_;}
    
    // loop::Nest constructLoopNest(const SymbolTable* symbol_table = nullptr) const;
    // size_t n_level() const {return loopnests_.size();}
    // const std::vector<loop::TileFlow::Descriptor>& get_loops() const {return loopnests_;}
};

class TransNode: public Node{
    std::string trans_name_;
    int trans_index_;
public:
    TransNode(config::CompoundConfigNode config);
    // TBD
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    // // void accept(Visitor* visitor) const {visitor->visitTrans(this);}
    // const std::string& get_trans_name() const {return trans_name_;}
    // int get_trans_index() const {return trans_index_;}
}

class ScopeNode: public Node {
public:
    enum type_t {
        Sequential,
        Sharing,
        Parallel,
        Pipeline
    };
    ScopeNode(config::CompoundConfigNode config);
    void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    // void accept(Visitor* visitor) const {visitor->visitScope(this);}
    ScopeNode::type_t get_scope_type() const {return type;}

private: 
    ScopeNode::type_t type;
    
};

class OpNode: public Node {
    std::string op_name_;
    int op_index_;
    // std::shared_ptr<problem::TileFlow::Workload> p_workload;
public:
    OpNode(config::CompoundConfigNode config);
    void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    const std::string & get_name() const {return op_name_;}
    // void accept(Visitor* visitor) const {visitor->visitOp(this);}
    // const std::shared_ptr<problem::TileFlow::Workload>& get_workload() const {return p_workload;}
};

class ExpMapping: public Mapping{
private:
    std::map<std::string, StringMap> ExpFanoutXMap;
    std::map<std::string, StringMap> ExpFanoutYMap;
    Node* root;

public:
    ExpMapping(){};
   
    // Parse
    void ParseFanoutMap(model::TileExp::Graph& arch_specs); // parse fanout map to ExpFanoutXMap and ExpFanoutYMap

    // Get
    std::map<std::string, StringMap> GetFanoutXMap() const { return ExpFanoutXMap; }
    std::map<std::string, StringMap> GetFanoutYMap() const { return ExpFanoutYMap; }

    // Print
    void Print();
}


} // end namespace TileExp

} // end namespace mapping