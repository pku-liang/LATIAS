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
    // p_workloads_ = &workloads;

    ExpMapping mapping;
    mapping.root = RecursiveParse(config);
    // mapping.fanoutX_map = arch_props_.FanoutX();
    // mapping.fanoutY_map = arch_props_.FanoutY();

    return mapping;
}

} // namespace TileExp

} // namespace mapping
