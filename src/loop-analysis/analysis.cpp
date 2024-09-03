#include "TileExp/loop-analysis/analysis.hpp"   
#include "TileExp/mapping/mapping.hpp"



namespace TileExp{

namespace Analysis{

EvaNode* EvaNode::BuildNewTree(const Node* node){
    EvaNode* new_node = new EvaNode(node);
    for(auto& child: node->children_){
        EvaNode* new_child = new_node->BuildNewTree(child);
        new_child->parent_ = new_node;
        new_node->children_.push_back(new_child);
    }
    return new_node;
}

// void ResetFunc::visitOp(OpNode* node){
//     node->inherit_loopnests_.clear();
//     node->in_out_tensors_.first.clear();
//     node->in_out_tensors_.second.clear();
//     for(auto& child: node->children_){
//         child->accept(this);
//     }
// }

// void ResetFunc::visitTile(TileNode* node){
//     node->inherit_loopnests_.clear();
//     node->in_out_tensors_.first.clear();
//     node->in_out_tensors_.second.clear();
//     for(auto& child: node->children_){
//         child->accept(this);
//     }
// }

// void ResetFunc::visitScope(ScopeNode* node){
//     node->inherit_loopnests_.clear();
//     node->in_out_tensors_.first.clear();
//     node->in_out_tensors_.second.clear();
//     for(auto& child: node->children_){
//         child->accept(this);
//     }
// }

// void ResectFunc::visitTrans(TransNode* node){
//     node->inherit_loopnests_.clear();
//     node->in_out_tensors_.first.clear();
//     node->in_out_tensors_.second.clear();
//     for(auto& child: node->children_){
//         child->accept(this);
//     }
// }


} // namespace Analysis
} // namespace TileExp