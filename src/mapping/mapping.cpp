#pragma once 

#include "model/engine.hpp"
#include "compound-config/compound-config.hpp"
#include "mapping/mapping.hpp"

#include "TileExp/common.hpp"
#include "TileExp/problem/problem.hpp"


namespace mapping{

namespace TileExp{

const std::unordered_map<Node::type_t, std::string> Node::type2name_ = {
    {Node::Tile, "Tile"},
    {Node::Op, "Op"},
    {Node::Scope, "Scope"}
};



}
}
