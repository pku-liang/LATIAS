#pragma once 

#include "mapping/loop.hpp"

namespace loop {

namespace TileExp{

    class Descriptor: public loop::Descriptor {
    public: 
        std::string name_;
        void Print(std::ostream& out, bool long_form = true) const;
    };

} // namespace TileExp 

} // namespace loop 

