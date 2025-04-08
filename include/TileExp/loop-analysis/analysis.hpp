#pragma once 

#include <vector> 
#include <stack>

#include "model/engine.hpp"
#include "TileExp/problem/problem.hpp"
#include "TileExp/mapping/mapping.hpp"
#include "TileExp/model/graph.hpp"
#include "TileExp/mapper/expr.hpp"
#include "TileExp/model/hardware.hpp"
#include "TileExp/model/interconnection.hpp"


using mapping::TileExp::Visitor;
using mapping::TileExp::Node;
using mapping::TileExp::OpNode;
using mapping::TileExp::TileNode;
using mapping::TileExp::ScopeNode;
using mapping::TileExp::TransNode;

using TileExp::DimRange;



namespace TileExp{

namespace Analysis{

typedef std::string DimName;

class SingleMemLevel{
    public:
    std::uint64_t max_word_num_; // block_size
    std::uint64_t word_bit_width_;
    // current exist tensors
    std::vector<TensorMap> stable_tensors_;
    std::vector<TensorMap> unstable_tensors_;
};

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

};

class EvaNode{
    public:
    const Node* ori_node_;
    std::vector<loop::TileExp::Descriptor> loopnests_;
    // private:
    std::vector<std::vector<loop::TileExp::Descriptor>> inherit_loopnests_; // inherit from the parent nodes
    std::vector<TensorMap> input_tensors_;
    std::vector<TensorMap> last_input_tensors_;
    std::vector<TensorMap> output_tensors_;
    // std::vector<TensorMap> last_output_tensors_;
    mutable EvaNode* parent_ = nullptr;
    std::vector<EvaNode*> children_;
    bool analysis_fine_gran_ = true;
    std::map<std::string, int> dim_offset_;
    
    public:
    EvaNode(const Node* node): ori_node_(node){
        loopnests_ = node->loopnests_;
        reset();
    }
    EvaNode* BuildNewTree(const Node* node);
    

    void set_parent(EvaNode* parent){ parent_ = parent; }
    void add_child(EvaNode* child){ children_.push_back(child); } 
    EvaNode* get_parent() { return parent_; }
    std::vector<EvaNode*> get_children() { return children_; }
    void add_inherit_loopnest(std::vector<loop::TileExp::Descriptor> loopnest){ inherit_loopnests_.push_back(loopnest); }
    std::vector<std::vector<loop::TileExp::Descriptor>> get_inherit_loopnest(){ return inherit_loopnests_; }
    // void clear_IO_tensors(){ 
    //     input_tensors_.clear(); 
    //     output_tensors_.clear(); 
    // }
    void InitDimOffset();
    void add_dim_offset(std::string dim_name, int dim_value){
        dim_offset_[dim_name] = dim_value;
    }
    std::map<std::string, int> get_dim_offset(){ return dim_offset_; }
    void add_input_tensors(TensorMap tensormap){ input_tensors_.push_back(tensormap); }
    void add_output_tensors(TensorMap tensormap){ output_tensors_.push_back(tensormap); }
    std::vector<TensorMap> get_input_tensors(){ return input_tensors_; }
    std::vector<TensorMap> get_output_tensors(){ return output_tensors_; }

    // reset loopnest, tensors
    void reset(){
        inherit_loopnests_.clear();
        input_tensors_.clear();
        last_input_tensors_.clear();
        output_tensors_.clear();
    }

    void PrintLoop() const;
};

class Evaluator{
    public:
    std::uint64_t cycle_;
    // double energy_;    
    const problem::TileExp::Workloads& workloads_;
    const mapping::TileExp::InterMapping& mapping_;
    const Hardware::ArchTopology::ArchTopo& arch_topo_;
    const Hardware::InterConnection::InterCon& intercon_;
    // const model::TileExp::Graph& graph_;
    // const model::Engine::Specs arch_specs_;
    EvaNode* eva_root_;
    const Node* root_;

    std::vector<MemLevel> mem_levels_;

    std::uint64_t data_movements_ = 0; // bits
    double latency_ = 0.0;
    double energy_ = 0.0;

    Evaluator(const problem::TileExp::Workloads& workloads,
        const mapping::TileExp::InterMapping& mapping, // tree-based mapping
        const Hardware::ArchTopology::ArchTopo& arch_topo,
        const Hardware::InterConnection::InterCon& intercon)
        // const model::TileExp::Graph& graph, 
        // const model::Engine::Specs arch_specs)
        : workloads_(workloads), mapping_(mapping), arch_topo_(arch_topo), intercon_(intercon){
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
    void analysis_latias();

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

class PerfAnalysis: public Visitor{
    public:
    Evaluator& evaluator_;
    EvaNode* current_node_;
    // for store the current loop info
    std::vector<std::vector<loop::TileExp::Descriptor>> loop_vector_all_;
    // for store all the tensor info
    std::vector<std::vector<TensorMap>> tensor_vector_all_;
    std::vector<DimRange> dim_range_tmp_;
    std::vector<TensorMap> tensor_map_tmp_;
    int current_tile_idx = 0;
    int data_movement = 0;
        
    // 递归循环，用于表示不同loop dimension
    void RecursiveLoop(const Node* node, unsigned current_loop_idx, bool is_last_loop);
    // 获取当前循环下，每一个dimension的范围
    std::vector<DimRange> computeRange(const std::vector<std::vector<loop::TileExp::Descriptor>> loop_vector);
    // 获取当前循环下，相关的tensor的范围
    std::vector<TensorMap> computeTensorRange(std::vector<DimRange>& dim_range);
    // 计算当前循环下，tensor变化的范围，并更新data_movement
    void diffTensorRange(const std::vector<DimRange>& tensor_range);
    // 计算在对应数据搬运量下的延迟，每一个延迟包括搬入，计算，搬出，或搬入、计算
    std::vector<int> computeLatency(std::string source, std::string target, int data_movement);
    std::vector<int> scopeLatency();
    std::vector<int> tileLatency();
    std::vector<int> transLatency();

    bool is_print_ = true;
    void visitScope(const ScopeNode* node) override;
    void visitTile(const TileNode* node) override;
    void visitOp(const OpNode* node) override;
    void visitTrans(const TransNode* node) override;
    void run(const Node* root) override;
    friend class Evaluator;
    PerfAnalysis(Evaluator& evaluator, EvaNode* current_node): 
        evaluator_(evaluator), current_node_(current_node){}
};



class SimAnalysis: public Visitor{
    public:
    Evaluator& evaluator_;
    EvaNode* current_node_;
    bool is_print_ = true;
    bool is_init_ = false;
    bool is_get_offset_ = false;
    std::vector<bool> vec_last_loop_;
    std::vector<loop::TileExp::Descriptor> current_loop_state_;
    std::map<std::string, std::vector<int>> dim_offset_all_;
    std::vector< std::vector<TensorMap> > current_input_tensors_;

    std::vector<std::string> GetInOutTensor(std::string name);
    std::map<std::string, std::vector<std::string> > GetDimName(std::vector<std::string> tensorName);
    bool isLastLoop(std::vector<bool> vec_last_loop);
    void visitTileLoop(const Node* node, unsigned current_loop_idx, bool is_last_loop);
    void visitScopeLoop(const Node* node);
    void visitOpLoop(const Node* node);
    void visitTransLoop();
    void visitScope(const ScopeNode* node) override;
    void visitTile(const TileNode* node) override;
    void visitOp(const OpNode* node) override;
    void visitTrans(const TransNode* node) override;
    void run(const Node* root) override;
    friend class Evaluator;
    SimAnalysis(Evaluator& evaluator, EvaNode* current_node): 
        evaluator_(evaluator), current_node_(current_node){}
    void Init(const Node* node);
    void getOffset(const Node* node);
};

class GetMemInfo: public Visitor{
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
    GetMemInfo(Evaluator& evaluator, EvaNode* current_node): evaluator_(evaluator), current_node_(current_node){}
};

} // namespace Analysis
} // namespace TileExp