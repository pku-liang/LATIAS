#pragma once 

#include "model/engine.hpp"
#include "compound-config/compound-config.hpp"
#include "mapping/mapping.hpp"

#include "TileExp/common.hpp"
#include "TileExp/problem/problem.hpp"
#include "TileExp/model/graph.hpp"

#include "TileExp/mapper/expr.hpp"
#include "TileExp/mapping/loop.hpp"
#include "TileExp/mapper/symbol.hpp"

using Symbol::SymbolTable;

namespace mapping{

extern std::map<std::string, unsigned> LevelName2IdxMap;

namespace TileExp{

void tolower(std::string& str);

typedef std::unordered_map<std::string, std::string> StringMap;
extern const problem::TileExp::Workloads* p_workloads_;


class Node {
public: 
    enum dataflow_mode{
        Forward,
        Write_back
    };
    enum type_t{
        Tile,
        Op,
        Scope,
        Trans // TBD
    };
protected:
    static const std::unordered_map<type_t, std::string> type2name_; 
    type_t type_;
    std::string name_;
    std::string target_level_name;
    unsigned target_level_id;
    mutable const Node* parent_ = nullptr;
    std::vector<const Node*> children_;
    mutable std::string storage_level_name_;
    mutable unsigned storage_level_ = unsigned(-1);
    std::vector<std::string> bypassed_; // decide whether to bypass this node
    bool profile_ = true;

    dataflow_mode dataflow_mode_;
    static const std::unordered_map<std::string, dataflow_mode> name2dataflow_mode_;

    // mutable ActiveTensor active_tensors_;

public:
    Node(type_t, config::CompoundConfigNode config);

    unsigned get_storage_level() const { return storage_level_; };
    std::string get_storage_name() const { return storage_level_name_; };
    std::string get_name() const { return name_; };
    std::string get_target_level_name() const { return target_level_name; };
    std::vector<const Node*> get_children() const { return children_; };
    type_t get_type() const { return type_; };

    // parser --  loop bounds {name, [low, high]}
    std::unordered_map<std::string, std::pair<int, int> > ParseFactors(const std::string& buffer);
    std::vector<std::string> ParsePermutations(const std::string & buffer);
    void ParseStorageLevel(config::CompoundConfigNode directive); 
    void ParseStorageLevel(config::CompoundConfigNode directive, model::Engine::Specs arch_specs); // Ray

    // virtual void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const; // TBD
    void add_child(const Node* child);
    void set_parent(const Node* parent) const {parent_ = parent;}

    void Print() const;
    
};


class TileNode: public Node {
public:
    enum type_t {
        Temporal,
        Spatial
    };
private:

    // describe current tile node's loop factor -- support var
    std::vector<loop::TileExp::Descriptor> loopnests_; 
    TileNode::type_t type_;
    bool multicast_enabled_ = true;
    // model::Engine::Specs arch_sepcs_;

public:
    TileNode(config::CompoundConfigNode config);
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, // TBD 
    //         std::ostream& = std::cout) const override;
    // void accept(Visitor* visitor) const {visitor->visitTile(this);} // TBD
    bool is_spatial() const {return type_ == Spatial;}
    bool is_multicast_enabled() const {return multicast_enabled_;}
    TileNode::type_t get_tile_type() const {return type_;}
    
    loop::Nest constructLoopNest(const SymbolTable* symbol_table = nullptr) const; // TBD
    size_t n_level() const {return loopnests_.size();}
    const std::vector<loop::TileExp::Descriptor>& get_loops() const {return loopnests_;}
};


class TransNode: public Node{
    std::string trans_name_;
    int trans_index_;
public:
    TransNode(config::CompoundConfigNode config);
    // TBD
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    // void accept(Visitor* visitor) const {visitor->visitTrans(this);}
    const std::string& get_trans_name() const {return trans_name_;}
    int get_trans_index() const {return trans_index_;}
};

class ScopeNode: public Node {
public:
    enum type_t {
        Sequential,
        Sharing,
        Parallel,
        Pipeline
    };
    ScopeNode(config::CompoundConfigNode config);
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    // void accept(Visitor* visitor) const {visitor->visitScope(this);} // TBD
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
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    const std::string & get_name() const {return op_name_;}
    // void accept(Visitor* visitor) const {visitor->visitOp(this);} // TBD
    // const std::shared_ptr<problem::TileFlow::Workload>& get_workload() const {return p_workload;}
};

// get mapping tree and fanout XY map
struct ExpMapping: public Mapping{

    std::map<std::string, StringMap> ExpFanoutXMap;
    std::map<std::string, StringMap> ExpFanoutYMap;
    Node* root;
    model::Engine::Specs arch_specs_;

    // function 
    ExpMapping(){};
   
    // Parse
    void ParseFanoutMap(model::TileExp::Graph& arch_specs); // parse fanout map to ExpFanoutXMap and ExpFanoutYMap

    // Get
    std::map<std::string, StringMap> GetFanoutXMap() const { return ExpFanoutXMap; }
    std::map<std::string, StringMap> GetFanoutYMap() const { return ExpFanoutYMap; }

    // Print
    void Print() const;
};


} // end namespace TileExp

} // end namespace mapping