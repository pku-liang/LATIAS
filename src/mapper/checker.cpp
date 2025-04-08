// 

#include "TileExp/mapper/checker.hpp"

namespace Check{
namespace TileExp {

// void Checker::check_memory_resource_constraint() const {
//     MemoryResourceConstraint pass(graph_);
//     pass.ParseMemoryMap();
//     pass.run(mapping_.root);
// }

void Checker::check() const {
    // check memory resource constraint
    if(enable_mem_check_) {
        // check_memory_resource_constraint();
    }
}

// void MemoryResourceConstraint::ParseMemoryMap() {
    
// }

} // namespace Check
} // namespace TileExp