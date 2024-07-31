#pragma once 

#include <vector> 

#include "TileExp/mapping/mapping.hpp"

// Goal: verify the correctness of current mapping under arch and workload
// rule1: memory resource constraint -- in paper, different level has different memory resource -- TransNode
// rule2: resource constraint

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
    const problem::TileExp::Mapping& mapping_;
    const model::TileExp::Graph& graph_;
    bool enable_mem_check_ = true;
    bool enable_spatial_check_ = true;
    bool enable_loopcount_check_ = true;
    bool enable_operation_check_ = true;

    public:
    Checker(const problem::TileExp::Workloads& workloads,
        const mapping::TileExp::Mapping& mapping,
        const model::Graph& graph,
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


}

} // namespace Checker
} // namespace TileExp