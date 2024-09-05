#pragma once 

#include <vector> 

#include "model/engine.hpp"
#include "TileExp/problem/problem.hpp"
#include "TileExp/mapping/mapping.hpp"
#include "TileExp/model/graph.hpp"
#include "TileExp/mapper/expr.hpp"

using mapping::TileExp::Visitor;
using mapping::TileExp::Node;
using mapping::TileExp::OpNode;
using mapping::TileExp::TileNode;
using mapping::TileExp::ScopeNode;
using mapping::TileExp::TransNode;

using TileExp::DimRange;

namespace TileExp{

namespace Analysis{

class SingleMemLevel{
    public:
    std::uint64_t max_word_num_; // block_size
    std::uint64_t word_bit_width_;
    // current exist tensors
    std::vector<TensorMap> stable_tensors_;
    std::vector<TensorMap> unstable_tensors_;
}

// DFS arch tree
class MemLevel{ // for spatial level
    public:
    std::string name_;
    std::uint64_t meshX_;
    std::uint64_t meshY_;
    double read_bandwidth_;
    double write_bandwidth_;
    const model::BufferLevel::Specs& ori_level_specs_;

    std::vector<std::vector<SingleMemLevel> > mem_level_; // instance array

}

class EvaNode{
    public:
    const Node* ori_node_;
    private:
    std::vector<std::vector<loop::TileExp::Descriptor>> inherit_loopnests_; // inherit from the parent nodes
    std::pair<std::vector<TensorMap>, std::vector<TensorMap> > in_out_tensors_;
    mutable EvaNode* parent_ = nullptr;
    std::vector<EvaNode*> children_;
    bool analysis_fine_gran_ = true;
    
    public:
    EvaNode(const Node* node): ori_node_(node){
        reset();
    }
    EvaNode* BuildNewTree(const Node* node);

    void set_parent(EvaNode* parent){ parent_ = parent; }
    void add_child(EvaNode* child){ children_.push_back(child); } 
    EvaNode* get_parent() { return parent_; }
    std::vector<EvaNode*> get_children() { return children_; }
    void add_inherit_loopnest(std::vector<loop::TileExp::Descriptor> loopnest){ inherit_loopnests_.push_back(loopnest); }
    std::vector<std::vector<loop::TileExp::Descriptor>> get_inherit_loopnest(){ return inherit_loopnests_; }

    void reset(){
        inherit_loopnests_.clear();
        in_out_tensors_.first.clear();
        in_out_tensors_.second.clear();
    }

    void Print() const;
};

class Evaluator{
    public:
    std::uint64_t cycle_;
    double energy_;    
    const problem::TileExp::Workloads& workloads_;
    const mapping::TileExp::ExpMapping& mapping_;
    const model::TileExp::Graph& graph_;
    const model::Engine::Specs arch_specs_;
    EvaNode* eva_root_;
    const Node* root_;

    std::vector<MemLevel> mem_levels_;

    std::uint64_t data_movements_ = 0; // bits
    double latency_ = 0.0;
    double energy_ = 0.0;

    Evaluator(const problem::TileExp::Workloads& workloads,
        const mapping::TileExp::ExpMapping& mapping, // tree-based mapping
        const model::TileExp::Graph& graph, 
        const model::Engine::Specs arch_specs)
        : workloads_(workloads), mapping_(mapping), graph_(graph), arch_specs_(arch_specs){
            root_ = mapping_.root;
            eva_root_ = new EvaNode(mapping_.root);
            eva_root_->set_parent(nullptr);
            for (auto& child: mapping_.root->children_){
                EvaNode* new_child = eva_root_->BuildNewTree(child);
                new_child->set_parent(eva_root_);
                eva_root_->add_child(new_child);
            }
            // std::cout << "**** EvaNode ****" <<std::endl;
            // eva_root_->Print();
            // std::cout << "**** End EvaNode ****" <<std::endl;
        }

    void reset(){
        eva_root_->reset();
        for(auto& child: eva_root_->get_children()){
            child->reset();
        }
    };

    void evaluate();

    // parse loop info for each EvaNode
    void get_loop_count();

    // parse memory info for each memory level
    void get_mem_info();

    // analysis data movement, latency
    void analysis();

    // friend class ResetFunc;
    // void evaluate_loop_count() const;
    // void evaluate_spatial_tile_size() const;
    // void evaluate_operation_type() const;
    // void evaluate_resource_constraint() const;
};


class GetLoopCount: public Visitor{
    public:
    Evaluator& evaluator_;
    EvaNode* current_node_;
    bool is_print_ = true;
    void visitScope(const ScopeNode* node) override;
    void visitTile(const TileNode* node) override;
    void visitOp(const OpNode* node) override;
    void visitTrans(const TransNode* node) override;
    void run(const Node* root) override;
    friend class Evaluator;
    GetLoopCount(Evaluator& evaluator, EvaNode* current_node): 
        evaluator_(evaluator), current_node_(current_node){}
    void IsPrint(bool is_print){ is_print_ = is_print; }
    std::map<std::string, int32_t> get_loop_count(std::vector<std::vector<loop::TileExp::Descriptor>> input_loopnests);
};

class GetMemInfo: public Visitor{
    public:
    Evaluator& evaluator_;
    bool is_print_ = true;
    void visitScope(const ScopeNode* node) override;
    void visitTile(const TileNode* node) override;
    void visitOp(const OpNode* node) override;
    void visitTrans(const TransNode* node) override;
    void run(const Node* root) override;
    friend class Evaluator;
    GetMemInfo(Evaluator& evaluator): evaluator_(evaluator){}
}

} // namespace Analysis
} // namespace TileExp