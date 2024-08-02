#pragma once 

#include <vector> 

#include "TileExp/mapping/mapping.hpp"

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


namespace TileExp{
namespace Check{

class Checker{
    private:
    const problem::TileExp::Workloads& workloads_;
    const mapping::TileExp::ExpMapping& mapping_;
    const model::TileExp::Graph& graph_;
    bool enable_mem_check_ = true;
    bool enable_spatial_check_ = true;
    bool enable_loopcount_check_ = true;
    bool enable_operation_check_ = true;

    public:
    Checker(const problem::TileExp::Workloads& workloads,
        const mapping::TileExp::ExpMapping& mapping, // tree-based mapping
        const model::TileExp::Graph& graph, // architecture
        bool enable_mem_check = true,
        bool enable_spatial_check = true,
        bool enable_loopcount_check = true,
        bool enable_operation_check = true)
        : workloads_(workloads), mapping_(mapping), graph_(graph), 
        enable_mem_check_(enable_mem_check),
        enable_spatial_check_(enable_spatial_check),
        enable_loopcount_check_(enable_loopcount_check),
        enable_operation_check_(enable_operation_check){}
    
    ~Checker(){}

    void check() const {}

};

// rule1: memory resource constraint -- bottom-top
// in paper, different level has different memory resource -- consider TransNode
class MemoryResourceConstraint: public Visitor{
    std::map<std::string, unsigned> mem_map_; // store all the memory resource
    std::map<std::string, unsigned> current_mem_map_; // store current usable mem resource
    const model::TileExp::Graph& graph_;

    void visitTile(const TileNode*) override;
    void visitScope(const ScopeNode*) override;
    void visitOp(const OpNode*) override; 
    void visitTrans(const TransNode*) override;

    // parse memory map from graph
    void ParseMemoryMap(model::TileExp::Graph& graph);

    void run(const Node*) override;
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
}

} // namespace Checker
} // namespace TileExp