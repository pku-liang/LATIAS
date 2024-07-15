#pragma once 

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <functional>
#include <iostream>

#include "TileExp/mapper/symbol.hpp"

namespace TileExp {
    const int ERROR_OUT=1;
    const int NORMAL_OUT=2;

    typedef size_t num_t;

    struct Constraint;

    

    


    // struct Constraint {
    //     enum {
    //         LOOPCOUNT,
    //         MEM, 
    //         SPATIAL
    //     }type_; 
    //     std::shared_ptr<Expr> expr;
    //     std::string msg;
    //     std::string short_msg = "";
    // };
}
