#include "TileExp/loop-analysis/analysis.hpp"   
#include "TileExp/mapping/mapping.hpp"

namespace TileExp{

namespace Analysis{

std::string nameSub(std::string input){
    size_t pos = input.find("::");
    if (pos != std::string::npos) {
        return input.substr(pos + 2); // +2 是因为 "::" 的长度是 2
    }
    return input;
}

EvaNode* EvaNode::BuildNewTree(const Node* node){
    EvaNode* new_node = new EvaNode(node);
    for(auto& child: node->children_){
        EvaNode* new_child = new_node->BuildNewTree(child);
        new_child->set_parent(new_node);
        new_node->add_child(new_child);
    }
    return new_node;
}

void EvaNode::printLoop() const {
    if (ori_node_->get_type() == Node::Scope) {
        std::cout << "Target Mem Level: No (Scope)" <<
                     ", Node type: " << ori_node_->get_name() << std::endl;
    }
    else {
        if (ori_node_->get_type() == Node::Trans) {
            std::cout << "Target Mem Level: " << ori_node_->get_target_level_name();
            std::cout << ", Output Tensor: ";
            for (auto child: this->output_tensors_) {
                std::cout << child.first;
            }
            std::cout << std::endl;
        }
        else{
            std::cout << "Target Mem Level: " << ori_node_->get_target_level_name();
            std::cout << ", Input Tensor: ";
            for (auto child: this->input_tensors_) {
                std::cout << child.first;
            }
            std::cout << ", Output Tensor: ";
            for (auto child: this->output_tensors_) {
                std::cout << child.first;
            }
            std::cout << std::endl;
        }
    }
    for (auto child: children_) {
        child->printLoop();
    }
}

void EvaNode::initDimOffset(){
    for(auto dim: ori_node_->loopnests_){
        dim_offset_[dim.name_] = 1;
    }
}

// 找到当前节点下对应维度，最近的offset
std::pair<StEd_pair, bool> BFSOffsetLoop(EvaNode* node, std::string dim_name){
    if (!node) TILEEXP_ASSERT(false, "BFS input node is nullptr");

    std::queue<EvaNode*> q;
    q.push(node);

    while (!q.empty()) {
        EvaNode* curr = q.front();
        q.pop();

        if (curr->get_dim_offset().count(dim_name)){
            StEd offset_pair(curr->get_dim_offset()[dim_name], curr->get_last_dim_offset()[dim_name]);
            StEd loop_pair;
            for (auto tmp_loop: curr->loopnests_){
                if (tmp_loop.name_ == dim_name){
                    loop_pair = StEd(tmp_loop.end, tmp_loop.residual_end);
                }
            }
            StEd_pair result_pair(offset_pair, loop_pair);
            return std::pair<StEd_pair, bool>(result_pair, true);
        }

        for (auto child : curr->get_children()) {
            if (child) q.push(child);
        }
    }
    // TILEEXP_ASSERT(false, "BFS fail");
    return std::pair<StEd_pair, bool>(StEd_pair(), false);
}


void Evaluator::evaluate(){
    std::cout << "======== Evaluate ========" <<std::endl;
    reset();
    get_loop_count();
    init_analysis();
    analysis_latias();
    // get_mem_info();
    // analysis();
    std::cout << "======== End Evaluate ========" <<std::endl;
}

void Evaluator::get_mem_info(){
    // GetMemInfo pass_(*this, eva_root_);
    // pass_.run(root_);
}

// 初始化pass
void Evaluator::init_analysis(){
    InitAnalysis pass_(*this, eva_root_);
    pass_.run(root_);
    leaf_vec_ = pass_.getLeafVec();
    if (TileExp::verbose_level > 0) {
        std::cout << "**** Tensor Path ****" <<std::endl;
        eva_root_->printLoop();
        std::cout << "**** End Tensor Path ****" <<std::endl;
    }
}

// 实际分析pass，得出数据搬运量和延迟的数据
void Evaluator::analysis_latias(){
    PerfAnalysis pass_(*this, eva_root_);
    pass_.setLeafVec(leaf_vec_);
    pass_.run(root_);
    data_movements_ = pass_.data_movements_;
    latency_ = pass_.current_node_->process_latency_;
}

// 确保维度匹配
void Evaluator::get_loop_count(){
    GetLoopCount pass_(*this, eva_root_);
    pass_.run(root_);
}

} // end namespace
}