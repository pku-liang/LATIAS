#pragma once 

#include <vector> 

#include "TileExp/mapping/mapping.hpp"
#include "TileExp/model/hardware.hpp"
#include "TileExp/model/interconnection.hpp"

// Goal: verify the correctness of current mapping under arch and workloads
// rule1: memory resource constraint -- in paper, different level has different memory resource -- consider TransNode
// rule2: resource constraint -- ensure that spatial tile is not too large
// rule3: compute gramma -- for workload
// rule4: loop count
// rule5: computing units type -- operation type

using mapping::TileExp::Node;
using mapping::TileExp::OpNode;
using mapping::TileExp::TileNode;
using mapping::TileExp::ScopeNode;
using mapping::TileExp::TransNode;
using mapping::TileExp::Visitor;


namespace Check{

namespace TileExp{

// std::list<std::string> ElementwiseOpList = {
//     "add", "sub", "mul", "div", "max", "min", "abs", "neg",
//     "and", "or", "xor", "not", "eq", "ne", "lt", "le",
//     "gt", "ge", "exp", "relu"
// };

class Checker{
    private:
    const problem::TileExp::Workloads& workloads_;
    const mapping::TileExp::InterMapping& mapping_;
    const Hardware::ArchTopology::ArchTopo& arch_topo_;
    const Hardware::InterConnection::InterCon& intercon_;

    // const model::TileExp::Graph& graph_;
    bool enable_mem_check_ = true;
    bool enable_spatial_check_ = true;
    bool enable_loopcount_check_ = true;
    bool enable_operation_check_ = true;

    public:
    Checker(const problem::TileExp::Workloads& workloads,
        const mapping::TileExp::InterMapping& mapping, // tree-based mapping
        // const model::TileExp::Graph& graph, // architecture
        const Hardware::ArchTopology::ArchTopo& arch_topo, // architecture
        const Hardware::InterConnection::InterCon& intercon, // interconnection
        bool enable_mem_check = true,
        bool enable_spatial_check = true,
        bool enable_loopcount_check = true,
        bool enable_operation_check = true)
        : workloads_(workloads), mapping_(mapping), 
        arch_topo_(arch_topo), intercon_(intercon),
        enable_mem_check_(enable_mem_check),
        enable_spatial_check_(enable_spatial_check),
        enable_loopcount_check_(enable_loopcount_check),
        enable_operation_check_(enable_operation_check){}
    
    // void check_memory_resource_constraint() const;
    void chech_mapping_loop_constraint() const;
    
    ~Checker(){}

    void check() const;

};

// rule1: memory resource constraint -- bottom-top
// in paper, different level has different memory resource -- consider TransNode
class MemoryResourceConstraint: public Visitor{
    private:
    std::map<std::string, unsigned> mem_map_; // store all the memory resource
    std::map<std::string, unsigned> current_mem_map_; // store current usable mem resource
    const model::TileExp::Graph& graph_;

    public:
    // void visitTile(const TileNode*) override;
    // void visitScope(const ScopeNode*) override;
    // void visitOp(const OpNode*) override; 
    // void visitTrans(const TransNode*) override;

    // parse memory map from graph
    void ParseMemoryMap();

    MemoryResourceConstraint(const model::TileExp::Graph& graph): graph_(graph){}
};

// rule2: resource constraint -- ensure that spatial tile is not too large
// ensure that the leaf node of mapping must be OpNode or TransNode
class ResourceConstraint: public Visitor{
    void visitTile(const TileNode*) override;
    void visitScope(const ScopeNode*) override;
    void visitOp(const OpNode*) override;
    void visitTrans(const TransNode*) override;
};

// rule3: compute gramma -- for workload, verify the correctness of tensor shape (GEMM, EXP, REDUCTION)
class OperationConstraint: public Visitor{
    void visitOp(const OpNode*) override;
};

// rule4: loop count -- easily -- bottom-top
class LoopCountConstraint: public Visitor{
    void visitTile(const TileNode*) override;
    void visitOp(const OpNode*) override;
};

// rule5: computing units type -- operation type
class ComputingUnitsConstraint: public Visitor{
    void visitOp(const OpNode*) override;
};

} // namespace Checker
} // namespace TileExp