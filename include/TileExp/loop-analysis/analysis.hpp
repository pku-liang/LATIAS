#pragma once 

#include <vector> 

#include "model/engine.hpp"
#include "TileExp/problem/problem.hpp"
#include "TileExp/mapping/mapping.hpp"
#include "TileExp/model/graph.hpp"

using mapping::TileExp::Visitor;
using mapping::TileExp::Node;
using mapping::TileExp::OpNode;
using mapping::TileExp::TileNode;
using mapping::TileExp::ScopeNode;
using mapping::TileExp::TransNode;

namespace TileExp{

namespace Analysis{

class EvaNode{
    public:

    const Node* ori_node_;
    std::vector<std::vector<loop::TileExp::Descriptor>> inherit_loopnests_; // inherit from the parent nodes
    std::pair<std::vector<TensorMap>, std::vector<TensorMap> > in_out_tensors_;
    mutable EvaNode* parent_ = nullptr;
    std::vector<EvaNode*> children_;

    EvaNode(const Node* node): ori_node_(node){}
    EvaNode* BuildNewTree(const Node* node);

    void reset(){
        inherit_loopnests_.clear();
        in_out_tensors_.first.clear();
        in_out_tensors_.second.clear();
    }
};

class Evaluator{
    private:
    const problem::TileExp::Workloads& workloads_;
    const mapping::TileExp::ExpMapping& mapping_;
    const model::TileExp::Graph& graph_;
    const model::Engine::Specs arch_specs_;

    public:
    EvaNode* root_;
    Evaluator(const problem::TileExp::Workloads& workloads,
        const mapping::TileExp::ExpMapping& mapping, // tree-based mapping
        const model::TileExp::Graph& graph, 
        const model::Engine::Specs arch_specs)
        : workloads_(workloads), mapping_(mapping), graph_(graph), arch_specs_(arch_specs){
            root_ = new EvaNode(mapping_.root);
            root_->parent_ = nullptr;
            for (auto& child: mapping_.root->children_){
                EvaNode* new_child = root_->BuildNewTree(child);
                new_child->parent_ = root_;
                root_->children_.push_back(new_child);
            }
        }

    void reset(){
        root_->reset();
        for(auto& child: root_->children_){
            child->reset();
        }
    };

    friend class ResetFunc;
    // void evaluate_loop_count() const;
    // void evaluate_spatial_tile_size() const;
    // void evaluate_operation_type() const;
    // void evaluate_resource_constraint() const;
};


class ResetFunc: public Visitor{
    public:
    void visitScope(const ScopeNode* node) override;
    void visitTile(const TileNode* node) override;
    void visitOp(const OpNode* node) override;
    void visitTrans(const TransNode* node) override;
    friend class Evaluator;
};

} // namespace Analysis
} // namespace TileExp