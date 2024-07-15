#pragma once 

#include "mapping/loop.hpp"

namespace loop {

namespace TileExp{

    class Descriptor: public loop::Descriptor {
    public: 
        std::string name_;
        bool is_variable_;
        void Print(std::ostream& out, bool long_form = true) const; // TBD
    };

} // namespace TileExp 

} // namespace loop 

