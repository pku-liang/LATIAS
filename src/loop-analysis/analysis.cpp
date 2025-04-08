#include "TileExp/loop-analysis/analysis.hpp"   
#include "TileExp/mapping/mapping.hpp"



namespace TileExp{

namespace Analysis{

EvaNode* EvaNode::BuildNewTree(const Node* node){
    EvaNode* new_node = new EvaNode(node);
    for(auto& child: node->children_){
        EvaNode* new_child = new_node->BuildNewTree(child);
        new_child->set_parent(new_node);
        new_node->add_child(new_child);
    }
    return new_node;
}

void EvaNode::PrintLoop() const {
    if (ori_node_->get_type() == Node::Scope) {
        std::cout << "Target Mem Level: No (Scope)" <<
                     ", Node type: " << ori_node_->get_name() << std::endl;
    }
    else {
        if (ori_node_->get_type() == Node::Trans) 
            std::cout << "Target Mem Level: " << ori_node_->get_target_level_name() << ", Node type: " << ori_node_->get_name() << std::endl;
        else{
            std::cout << "Target Mem Level: " << ori_node_->get_target_level_name() << ", Node type: " << ori_node_->get_name();
            std::cout << ", Loop Nest:";
            for (auto loop: ori_node_->loopnests_) {
                std::cout << " " << loop.name_ << "[" << loop.start << ", " << loop.end << "(" << loop.residual_end << "), " << loop.stride << "]";
            }
            std::cout << std::endl;
        }
    }
    for (auto child: children_) {
        child->PrintLoop();
    }
}

void EvaNode::InitDimOffset(){
    for(auto dim: ori_node_->loopnests_){
        dim_offset_[dim.name_] = 1;
    }
}

void Evaluator::evaluate(){
    std::cout << "======== Evaluate ========" <<std::endl;
    reset();
    get_loop_count();
    // get_mem_info();
    // analysis();
    analysis_latias();
    std::cout << "======== End Evaluate ========" <<std::endl;
}

void Evaluator::get_mem_info(){
    // GetMemInfo pass_(*this, eva_root_);
    // pass_.run(root_);
}

void Evaluator::analysis(){
    SimAnalysis pass_(*this, eva_root_);
    pass_.run(root_);
}

void Evaluator::analysis_latias(){
    PerfAnalysis pass_(*this, eva_root_);
    pass_.run(root_);
}

void PerfAnalysis::run(const Node* root){
    // init
    // is_init_ = true;
    // root->accept(this);
    // is_init_ = false;
    // // get offset
    // is_get_offset_ = true;
    // root->accept(this);
    // is_get_offset_ = false;

    // analysis
    root->accept(this);
}

void PerfAnalysis::visitScope(const ScopeNode* node){
}

void PerfAnalysis::visitTile(const TileNode* node){
}

void PerfAnalysis::visitOp(const OpNode* node){
}

void PerfAnalysis::visitTrans(const TransNode* node){
}

// 递归循环，用于表示不同loop dimension
void PerfAnalysis::RecursiveLoop(const Node* node, unsigned current_loop_idx, bool is_last_loop){
    if (~is_last_loop){
        // 递归循环
        RecursiveLoop(node, current_loop_idx + 1, is_last_loop);
    }
    else{
        // go to child-node
        for (unsigned i = 0 ; i < node->get_children().size(); i++){
            current_node_ = current_node_->get_children()[i];
            node->get_children()[i]->accept(this);
        }
        current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
    }
}




void SimAnalysis::run(const Node* root){
    // init
    is_init_ = true;
    root->accept(this);
    is_init_ = false;
    // get offset
    is_get_offset_ = true;
    root->accept(this);
    is_get_offset_ = false;

    // compute tensor range
    root->accept(this);
}

// Init and Get loop offset, store in SimAnalysis
void SimAnalysis::Init(const Node* node){
    // current_node_->clear_IO_tensors();
    current_node_->InitDimOffset();

    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void SimAnalysis::getOffset(const Node* node){

    // node->loopnests_;
    // current_node_->

    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }

    std::map<std::string, int> tmp_dim_offset_;
    if (node->get_children().size() != 0){
        if(node->get_type() == Node::Scope){ }
        else{
            for (unsigned i = 0; i < node->get_children().size(); i++){
                auto child = node->get_children()[i];
                auto eva_child = current_node_->get_children()[i];
                if(child->get_type() == Node::Scope){
                    for (unsigned j = 0; j < child->get_children().size(); j++){
                        auto child2 = child->get_children()[j];
                        auto eva_child2 = eva_child->get_children()[j];
                        for (auto tmp_loop: child2->loopnests_){
                            current_node_->add_dim_offset(tmp_loop.name_, 
                                    tmp_loop.end * eva_child2->get_dim_offset()[tmp_loop.name_]);
                        }
                    }
                }
                else {
                    for (auto tmp_loop: child->loopnests_){
                        current_node_->add_dim_offset(tmp_loop.name_, 
                                                    tmp_loop.end * eva_child->get_dim_offset()[tmp_loop.name_]);
                    }
                }
            }
        }
    }
    else{
        if(node->get_type() == Node::Trans){ }
        else{
            for (auto tmp: node->loopnests_){
                current_node_->add_dim_offset(tmp.name_, 1);
            }
        }
    }

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

bool SimAnalysis::isLastLoop(std::vector<bool> vec_last_loop){
    bool tmp = true;
    for(auto last_loop: vec_last_loop){
        tmp = tmp && last_loop;
    }
    return tmp;
}

void SimAnalysis::visitTileLoop(const Node* node, unsigned current_loop_idx, bool is_last_loop){
                            // std::vector<loop::TileExp::Descriptor> loop_state){
    
    if (node->parent_ == nullptr || isLastLoop(vec_last_loop_)){ is_last_loop = true; }
    auto loop_bound = node->loopnests_.size();

    if (current_loop_idx == loop_bound){
        // 遍历全部child，获取tensor信息
        for (unsigned i = 0 ; i < node->get_children().size(); i++){
            current_node_ = current_node_->get_children()[i];
            node->get_children()[i]->accept(this);
        }
        // // 根据child的tensor信息以及父节点的scope信息，构建当前节点的tensormap
        // auto parent_type = current_node_->type_();
        // for (unsigned i = 0 ; i < node->get_children().size(); i++){
        //     auto tmp_node = current_node_->get_children()[i];
        // }
        return;
    }
    
    auto dim_name_ = current_node_->loopnests_[current_loop_idx].name_;

    int ori_start = current_node_->loopnests_[current_loop_idx].start;
    int* loop_start = &(current_node_->loopnests_[current_loop_idx].start);
    int loop_end;
    if (is_last_loop){
        loop_end = current_node_->loopnests_[current_loop_idx].end + 
                   current_node_->loopnests_[current_loop_idx].residual_end;
    }
    else{
        loop_end = current_node_->loopnests_[current_loop_idx].end;
    }
    int loop_stride = current_node_->loopnests_[current_loop_idx].stride;
    assert(loop_stride == 1);

    for (; *loop_start < loop_end; *loop_start += loop_stride){
        current_loop_state_.push_back(current_node_->loopnests_[current_loop_idx]);
        dim_offset_all_[dim_name_].push_back(current_node_->dim_offset_[dim_name_]);
        vec_last_loop_.push_back(*loop_start == loop_end - 1);

        visitTileLoop(node, current_loop_idx + 1, is_last_loop);
        
        vec_last_loop_.pop_back();
        dim_offset_all_[dim_name_].pop_back();
        current_loop_state_.pop_back();
    }
    if (current_loop_idx == 0){
        current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
    }
    // 还原初始start
    *loop_start = ori_start;
}


std::string NameSub(std::string input){
    size_t pos = input.find("::");
    if (pos != std::string::npos) {
        return input.substr(pos + 2); // +2 是因为 "::" 的长度是 2
    }
    return input;
}

std::vector<std::string> SimAnalysis::GetInOutTensor(std::string name){
    std::vector<std::string> res;
    auto workload = evaluator_.workloads_.get_workload(name);
    for (auto& dataSpaceName: workload->get_ins()){
        res.push_back(dataSpaceName);
    }
    res.push_back(workload->get_out());
    return res;
}

// last dimName is the output
std::map<std::string, std::vector<std::string> > SimAnalysis::GetDimName(std::vector<std::string> tensorName){
    // auto workload = evaluator_.workloads_.get_workload(name);
    auto common_shape = evaluator_.workloads_.get_shape();
    // input
    std::vector<std::string> tmp_dim_name;
    std::map<std::string, std::vector<std::string> > res_dim_name;
    for (auto& dataSpaceName: tensorName){
        tmp_dim_name.clear();
        auto dataSpaceID = common_shape.DataSpaceNameToID[dataSpaceName];
        auto& proj = common_shape.Projections[dataSpaceID];
        for (auto& expr: proj){
            for (auto& term: expr){
                tmp_dim_name.push_back(common_shape.FactorizedDimensionIDToName[term.second]);
            }
        }
        res_dim_name[dataSpaceName] = tmp_dim_name;
    }
    return res_dim_name;
}

// 计算data move的几个步骤
// 每一个op都会根据offset计算产生一个input和output tensor，以及每一个维度的循环带来的range，
// 作为data movement
// 每一个tile都会根据自身的tiling factor，对每一个child的io tensor进行维度变化
void SimAnalysis::visitOpLoop(const Node* node){
    
    auto loop_bound = node->loopnests_.size();

    for (unsigned i = 0; i < loop_bound; i++){ 
        current_loop_state_.push_back(current_node_->loopnests_[i]);
    }

    // auto offset = ;
    // for (auto loop_tmp : current_loop_state_){
    //     std::cout << loop_tmp.name_ << loop_tmp.start << "  ";
    // }
    // std::cout << std::endl;

    std::string name = NameSub(node->name_);
    std::vector<std::string> tensorName = GetInOutTensor(name);
    std::map<std::string, std::vector<std::string> > dimName = GetDimName(tensorName);

    // 每一个循环为当前OP节点添加一个tensormap
    DimRange dimRange;
    std::vector<DimRange> dimRangeVec;
    for(unsigned i = 0; i < tensorName.size(); i++){
        // output
        dimRangeVec.clear();
        if(i == tensorName.size() - 1){
            auto tensor = tensorName[i];
            auto tensor_dim = dimName[tensor];
            for(auto dim_name: tensor_dim){
                dimRange.dim_name_ = dim_name;
                dimRange.low_bound_ = 0;
                if(isLastLoop(vec_last_loop_)){
                    dimRange.high_bound_ = 0;
                }
                dimRangeVec.push_back(dimRange);
            }
            TensorMap tensorMap(tensor, tensor_dim, dimRangeVec);
            current_node_->output_tensors_.push_back(tensorMap);
        }
        // input
        else{   
            auto tensor = tensorName[i];
            auto tensor_dim = dimName[tensor];
            for(auto dim_name: tensor_dim){
                dimRange.dim_name_ = dim_name;
                dimRange.low_bound_ = 0;
                if(isLastLoop(vec_last_loop_)){
                    dimRange.high_bound_ = 0;
                }
                dimRangeVec.push_back(dimRange);
            }
            TensorMap tensorMap(tensor, tensor_dim, dimRangeVec);
            current_node_->input_tensors_.push_back(tensorMap);
        }
    }

    for (unsigned i = 0; i < loop_bound; i++){ 
        current_loop_state_.pop_back();
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void SimAnalysis::visitScopeLoop(const Node* node){

    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void SimAnalysis::visitTransLoop(){
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

// recursive for loop
// 第一个循环做嵌套的loopnest的循环，间隔为stride，首尾为start，end+residual
// 如果是spatial。。。。
// 第二个循环做child级别的，不同child之间的关系根据scope来
// 每一个child都需要在当前循环建立一个tensormap：这里根据input和output的不同会有不一样的data movement
// 需要特殊处理Op，scope和trans节点
// 只计算0->1,1->2的数据*(loop-1)
// 各个child的tensormap根据scope的关系得出一个大的tensormap为父节点的tensormap

void SimAnalysis::visitScope(const ScopeNode* node){
    if(is_init_){ Init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitScopeLoop(node);
}

void SimAnalysis::visitTile(const TileNode* node){
    if(is_init_){ Init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitTileLoop(node, 0, false);
}

void SimAnalysis::visitOp(const OpNode* node){
    if(is_init_){ Init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitOpLoop(node);
}

void SimAnalysis::visitTrans(const TransNode* node){
    if(is_init_){ Init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitTransLoop();
}

void Evaluator::get_loop_count(){
    GetLoopCount pass_(*this, eva_root_);
    pass_.run(root_);
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
    // for (auto& tmp: common_shape.FactorizedDimensionIDToName){
    //     std::cout << tmp.first << " " << tmp.second << std::endl;
    // }
    // std::cout << "factorized_bounds: " << std::endl;
    // for (auto& tmp: factorized_bounds){
    //     std::cout << tmp.first << " " << tmp.second << std::endl;
    // }

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

// get actual mapping loop count
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
        dim_results[dim.first] = 1;
        int tem_result = 1;
        for (unsigned i = 0; i < dim_ends[dim.first].size(); ++i){
            tem_result *= dim_ends[dim.first][i];
            // if (i == 0){ tem_result *= dim_ends[dim.first][i] - 1; }
            // else { tem_result *= dim_ends[dim.first][i]; }
        }
        dim_results[dim.first] = tem_result;
        unsigned loop_upbound = dim_residual_ends[dim.first].size();
        for (unsigned i = 0; i < loop_upbound; ++i){
            tem_result = dim_residual_ends[dim.first][i];
            for (unsigned j = i + 1; j < loop_upbound; j++){
                tem_result *= dim_ends[dim.first][j];
            }
            // for (unsigned j = i + 1; j < dim_residual_ends[dim.first].size(); ++j){
            //     if (j == i + 1 && j < dim_residual_ends[dim.first].size()) {
            //         tem_result *= dim_residual_ends[dim.first][j] - 1;
            //     }
            //     else { tem_result *= dim_ends[dim.first][j]; }
            // }
            dim_results[dim.first] += tem_result;
        } 
    }
    return dim_results;
}



// std::map<std::string, int> GetLoopCount::get_loop_count(
//     std::vector<std::vector<loop::TileExp::Descriptor>> input_loopnests){   
// }

} // namespace Analysis
} // namespace TileExp