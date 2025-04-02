#pragma once 

#include "model/engine.hpp"
#include "compound-config/compound-config.hpp"
#include "mapping/mapping.hpp"

#include "TileExp/common.hpp"
#include "TileExp/problem/problem.hpp"
#include "TileExp/model/graph.hpp"

#include "TileExp/mapper/expr.hpp"
#include "TileExp/mapper/symbol.hpp"
#include "TileExp/mapping/loop.hpp"
// #include "TileExp/mapping/mapping.hpp"


using Symbol::SymbolTable;
using TileExp::Range;
using TileExp::TensorMap; 

namespace mapping{

extern std::map<std::string, unsigned> LevelName2IdxMap;
extern std::map<std::string, std::string> LevelName2TypeMap;

namespace TileExp{

enum ScopeType {
    Sequential,
    Parallel,
    Sharing,
    Pipeline,
    Ring,
    Mesh
};

void tolower(std::string& str);

typedef std::unordered_map<std::string, std::string> StringMap;
typedef std::pair<float, float> MeshXYPair; // [meshX, meshY]
typedef std::map<std::string, MeshXYPair> TargetMeshXYMap; // fanout relatioship [target, meshXY]
typedef std::map<std::string, TargetMeshXYMap> ExpFanoutXYMap; // arch level XY [current, target]

typedef std::string DimName;

extern const problem::TileExp::Workloads* p_workloads_;


class Node;
class ScopeNode;
class TileNode;
class OpNode;
class TransNode;
struct ExpMapping;

class Visitor {
protected:
    virtual void visitScope(const ScopeNode*);
    virtual void visitTile(const TileNode*);
    virtual void visitOp(const OpNode*);
    virtual void visitTrans(const TransNode*);
    friend class TileNode;
    friend class ScopeNode;
    friend class OpNode;
    friend class TransNode;
public:
    virtual void run (const Node*);
};

struct ActiveTensor {
    std::set<problem::Shape::DataSpaceID> 
    read_tensors, fill_tensors, update_tensors, wb_tensors;
};

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
        Trans
    };
public:
    type_t type_;
    ScopeType scope_type_; // declare the scope type
    bool profile_ = true;
    std::string name_;
    std::string target_level_name;
    static const std::unordered_map<type_t, std::string> type2name_; 
    unsigned target_level_id;
    std::vector<loop::TileExp::Descriptor> loopnests_; 
    std::vector<const Node*> children_;
    std::vector<std::string> bypassed_; // decide whether to bypass this node
    // state for calculate data movement and latency
    // std::vector<std::vector<loop::TileExp::Descriptor>> inherit_loopnests_; // inherit from the parent nodes
    // std::pair<std::vector<TensorMap>, std::vector<TensorMap> > in_out_tensors_;
    mutable const Node* parent_ = nullptr;
    mutable std::string storage_level_name_;
    mutable unsigned storage_level_ = unsigned(-1);

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
    std::vector<loop::TileExp::Descriptor> get_loops() const {return loopnests_;}

    // parser --  loop bounds {name, [bound, residual bound]}
    std::unordered_map<std::string, std::pair<int, int> > ParseFactors(const std::string& buffer);
    std::vector<std::string> ParsePermutations(const std::string & buffer);
    void ParseStorageLevel(config::CompoundConfigNode directive); 
    void ParseStorageLevel(config::CompoundConfigNode directive, model::Engine::Specs arch_specs); // Ray

    // virtual void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const; // TBD
    void add_child(const Node* child);
    void set_parent(const Node* parent) const {parent_ = parent;}

    void Print() const;

    virtual void accept(Visitor* visitor) const = 0;
    friend class Visitor;

};


class TileNode: public Node {
public:
    enum type_t {
        Temporal,
        Spatial
    };
private:

    // describe current tile node's loop factor -- support var
    TileNode::type_t type_;
    bool multicast_enabled_ = true;
    // model::Engine::Specs arch_sepcs_;

public:
    TileNode(config::CompoundConfigNode config);
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, // TBD 
    //         std::ostream& = std::cout) const override;
    void accept(Visitor* visitor) const {visitor->visitTile(this);}
    bool is_spatial() const {return type_ == Spatial;}
    bool is_multicast_enabled() const {return multicast_enabled_;}
    TileNode::type_t get_tile_type() const {return type_;}
    
    loop::Nest constructLoopNest(const SymbolTable* symbol_table = nullptr) const; // TBD, currently do not use
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
    void accept(Visitor* visitor) const {visitor->visitTrans(this);}
    const std::string& get_trans_name() const {return trans_name_;}
    int get_trans_index() const {return trans_index_;}
};

class ScopeNode: public Node {
public:
    // enum type_t {
    //     Sequential,
    //     Sharing,
    //     Parallel,
    //     Pipeline
    // };
    ScopeNode(config::CompoundConfigNode config);
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    void accept(Visitor* visitor) const {visitor->visitScope(this);}
    // ScopeNode::type_t get_scope_type() const {return type;}
    ScopeType get_scope_type() const {return type;}

private: 
    ScopeType type;
    // ScopeNode::type_t type;
    
};

class OpNode: public Node {
    std::string op_name_;
    int op_index_; // useless
    std::shared_ptr<problem::TileExp::Workload> p_workload;
public:
    OpNode(config::CompoundConfigNode config);
    // void display(std::string prefix, bool recursive, const SymbolTable* = nullptr, std::ostream& = std::cout) const override;
    const std::string & get_name() const {return op_name_;}
    void accept(Visitor* visitor) const {visitor->visitOp(this);}
    const std::shared_ptr<problem::TileExp::Workload>& get_workload() const {return p_workload;}
};

// get mapping tree and fanout XY map
struct ExpMapping: public Mapping{

    ExpFanoutXYMap ExpFanoutXYMap_; // arch level XY
    Node* root;
    model::Engine::Specs arch_specs_;

    // function 
    ExpMapping(){};
    // Parse
    void ParseFanoutMap(model::TileExp::Graph& arch_specs); // parse fanout map to ExpFanoutXMap and ExpFanoutYMap
    MeshXYPair GetNodeMeshXY(std::shared_ptr<model::TileExp::GraphNode> ptr_node_,
                                        std::string type_);
    // Get
    ExpFanoutXYMap GetFanoutXYMap() const { return ExpFanoutXYMap_; };
    // Print
    void Print() const;
    void PrintFanoutMap() const;
};


} // end namespace TileExp

} // end namespace mapping