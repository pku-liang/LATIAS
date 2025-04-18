

#include "TileExp/mapper/expr.hpp"

namespace TileExp{

    int64_t calTensorMapDM(const TensorMap& after){
        TILEEXP_ASSERT(!after.dim_ranges_.empty(), "TensorMap is empty");
        int64_t map2_size = 1;
        for (const auto& pair : after.dim_ranges_) {
            auto dim_range = pair.second;
            int map2_dimlen = dim_range.high_bound_ - dim_range.low_bound_;
            map2_size *= map2_dimlen;
        }
        return map2_size;
    }

    int64_t calTensorMapDM(const TensorMap& before, const TensorMap& after){
        TILEEXP_ASSERT(before.tensor_dims_.size() == after.tensor_dims_.size(), "TensorMap size mismatch");
        TILEEXP_ASSERT(!after.dim_ranges_.empty(), "TensorMap is empty");
        
        int64_t map2_size = 1;
        int64_t overlap = 1;
        if (before.dim_ranges_.empty()) {
            for (const auto& pair : after.dim_ranges_) {
                auto dim_range = pair.second;
                int map2_dimlen = dim_range.high_bound_ - dim_range.low_bound_;
                map2_size *= map2_dimlen;
            }
            return map2_size;
        }
        
        for (const auto& pair1 : before.dim_ranges_) {
            auto it = after.dim_ranges_.find(pair1.first);
            if (it != after.dim_ranges_.end()) {
                auto dim_range1 = pair1.second;
                auto dim_range2 = it->second;
                int64_t high_bound1 = dim_range1.high_bound_;
                int64_t low_bound1 = dim_range1.low_bound_;
                int64_t high_bound2 = dim_range2.high_bound_;
                int64_t low_bound2 = dim_range2.low_bound_;
                int64_t map1_dimlen = high_bound1 - low_bound1;
                int64_t map2_dimlen = high_bound2 - low_bound2;
                map2_size *= map2_dimlen;
                
                if(low_bound1 < low_bound2) {
                    overlap *= std::max(std::min(high_bound1 - low_bound2, map2_dimlen), static_cast<int64_t>(0));
                } else {
                    overlap *= std::max(std::min(high_bound2 - low_bound1, map1_dimlen), static_cast<int64_t>(0));
                }
            }
        }
        return map2_size - overlap;
    }
}