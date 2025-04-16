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
    typedef unsigned tensor_bound;

    // struct Constraint;
    // struct Range;
    
    struct DimRange{
        std::string dim_name_; // tensor dimension
        int64_t low_bound_;
        int64_t high_bound_;
        std::string target_mem_level_;
        
    };

    struct TensorMap{
        std::string tensor_name_;
        std::vector<std::string> tensor_dims_; // tensor dimension
        std::vector<DimRange> tensor_ranges_;

        TensorMap() = default;
        TensorMap(std::string tensor_name, std::vector<std::string> tensor_dims):
            tensor_name_(tensor_name), tensor_dims_(tensor_dims) {};

        TensorMap(std::string tensor_name, 
            std::vector<std::string> tensor_dims,
            std::vector<DimRange> tensor_ranges):
            tensor_name_(tensor_name), tensor_dims_(tensor_dims), tensor_ranges_(tensor_ranges) {}

        void add_dim(); // TBD
        
    };

    // need overload TensorMap add function
    // TensorMap operator+(const TensorMap& tensor_map1, const TensorMap& tensor_map2);
    int64_t addTensorMap(const TensorMap& tensor_map1, const TensorMap& tensor_map2);

} // namespace TileExp
