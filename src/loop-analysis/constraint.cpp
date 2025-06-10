#include "TileExp/loop-analysis/analysis.hpp"   
#include "TileExp/mapping/mapping.hpp"

namespace TileExp{

namespace Analysis{

// ************************ constraint part *************************

// get actual mapping loop count -- TBD:需要变化为timeloop形式的表示 -- done
std::map<std::string, int32_t> GetLoopCount::get_loop_count(
    std::vector<std::vector<loop::TileExp::Descriptor>> input_loopnests){
    std::map<std::string, int32_t> dim_results;
    std::map<std::string, std::vector<int32_t>> dim_ends;
    std::map<std::string, std::vector<int32_t>> dim_residual_ends;
    for (auto& loopnest: input_loopnests){
        for (auto& dim: loopnest){
            if (!dim_ends.count(dim.name_)){
                dim_ends[dim.name_] = std::vector<int32_t>{dim.end};
                dim_residual_ends[dim.name_] = std::vector<int32_t>{dim.residual_end};
            }
            else{
                TILEEXP_ASSERT(dim.end >= dim.residual_end, "Dim end smaller than residual end");
                dim_ends[dim.name_].push_back(dim.end);
                dim_residual_ends[dim.name_].push_back(dim.residual_end);
            }
        }
    }
    for (auto& dim: dim_ends){
        auto dim_name = dim.first;
        dim_results[dim_name] = 1;
        for (unsigned i = 0; i < dim_ends[dim_name].size(); ++i){
            int tem_result = 1;
            for (unsigned j = i; j < dim_ends[dim_name].size(); ++j){
                if (i == j) continue;
                tem_result *= dim_ends[dim_name][j];
            }
            dim_results[dim_name] += tem_result * (dim_residual_ends[dim_name][i] - 1);
        }
    }
    return dim_results;
}


void GetLoopCount::run(const Node* root){
    root->accept(this);
}

void GetLoopCount::visitTile(const TileNode* node){
    if (current_node_->get_parent() != nullptr){
        for (auto& parent_loop_: current_node_->get_parent()->get_inherit_loopnest()){
            current_node_->add_inherit_loopnest(parent_loop_); // append parent node's loopnest
        }
    }
    current_node_->add_inherit_loopnest(node->loopnests_);

    TILEEXP_ASSERT(node->get_children().size() > 0, "Tile node should have at least one child");
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void GetLoopCount::visitScope(const ScopeNode* node){
    for (auto& parent_loop_: current_node_->get_parent()->get_inherit_loopnest()){
        current_node_->add_inherit_loopnest(parent_loop_); // append parent node's loopnest
    } 
    TILEEXP_ASSERT(node->get_children().size() > 0, "Scope node should have at least one child");
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void GetLoopCount::visitOp(const OpNode* node){
    for (auto& parent_loop_: current_node_->get_parent()->get_inherit_loopnest()){
        current_node_->add_inherit_loopnest(parent_loop_); // append parent node's loopnest
    }
    current_node_->add_inherit_loopnest(node->loopnests_);

    TILEEXP_ASSERT(node->get_children().size() == 0, "Op node should not have child");
    // check if the loop nest is legal
    auto common_shape = evaluator_.workloads_.get_shape();
    auto factorized_bounds = evaluator_.workloads_.get_factorized_bounds();

    auto mapping_loop_results = get_loop_count(current_node_->get_inherit_loopnest());
    for (auto& tmp: mapping_loop_results){
        auto dim_name = tmp.first;
        auto dim_id = common_shape.FactorizedDimensionNameToID[dim_name];
        TILEEXP_ASSERT(factorized_bounds[dim_id] == tmp.second, 
            "Dim " + dim_name + " of " + current_node_->ori_node_->name_ + " is not equal to the factorized bounds " + std::to_string(factorized_bounds[dim_id]));
    }
    std::cout << "Loop count of Op " + current_node_->ori_node_->name_ + " success!" << std::endl;

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void GetLoopCount::visitTrans(const TransNode* node){
    TILEEXP_ASSERT(node->get_children().size() == 0, "Trans node should not have child");
    for (auto& parent_loop_: current_node_->get_parent()->get_inherit_loopnest()){
        current_node_->add_inherit_loopnest(parent_loop_); // append parent node's loopnest
    }
    if(node){}
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

} // end namespace
}