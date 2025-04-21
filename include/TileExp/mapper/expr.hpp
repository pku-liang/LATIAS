#pragma once 

#include <unordered_map>
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <functional>
#include <iostream>
#include <utility>
#include <cstdint>
#include <algorithm>

#include "TileExp/mapper/symbol.hpp"
#include "TileExp/common.hpp"

namespace TileExp {
    const int ERROR_OUT=1;
    const int NORMAL_OUT=2;

    typedef size_t num_t;
    typedef unsigned tensor_bound;

    // struct Constraint;
    // struct Range;
    
    struct Latency {
        int64_t input_latency_ = 0;
        int64_t output_latency_ = 0;
        int64_t process_latency_ = 0;
        unsigned sub_latency_num_ = 0;

        Latency() = default;
        Latency(int64_t input_latency, int64_t output_latency, int64_t process_latency):
            input_latency_(input_latency), output_latency_(output_latency), process_latency_(process_latency) {};
        Latency(int64_t input_latency, int64_t process_latency):
            input_latency_(input_latency), process_latency_(process_latency) {};
    };

    struct DimRange{
        std::string dim_name_;
        int low_bound_ = 0;
        int high_bound_ = 0;
        std::string target_mem_level_;

        DimRange() = default;
        DimRange(std::string dim_name, std::pair<int, int> range):
            dim_name_(dim_name), low_bound_(range.first), high_bound_(range.second) {};
    };

    struct TensorMap{
        std::string tensor_name_;
        std::vector<std::string> tensor_dims_; // tensor dimension
        std::unordered_map<std::string, DimRange> dim_ranges_;

        TensorMap() = default;
        TensorMap(std::string tensor_name, std::vector<std::string> tensor_dims):
            tensor_name_(tensor_name), tensor_dims_(tensor_dims) {};

        TensorMap(std::string tensor_name, 
            std::vector<std::string> tensor_dims,
            std::unordered_map<std::string, DimRange> dim_ranges):
            tensor_name_(tensor_name), tensor_dims_(tensor_dims), dim_ranges_(dim_ranges) {}

        void add_dim(); // TBD
        
    };

    // need overload TensorMap add function
    // TensorMap operator+(const TensorMap& tensor_map1, const TensorMap& tensor_map2);
    // int64_t addTensorMap(const TensorMap& tensor_map1, const TensorMap& tensor_map2);
    int64_t calTensorMapDM(const TensorMap& before, const TensorMap& after);
    int64_t calTensorMapDM(const TensorMap& after);

} // namespace TileExp
