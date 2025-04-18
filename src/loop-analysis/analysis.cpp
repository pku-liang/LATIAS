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
        child->printLoop();
    }
}

void EvaNode::initDimOffset(){
    for(auto dim: ori_node_->loopnests_){
        dim_offset_[dim.name_] = 1;
    }
}

void Evaluator::evaluate(){
    std::cout << "======== Evaluate ========" <<std::endl;
    reset();
    get_loop_count();
    init_analysis();
    // get_mem_info();
    // analysis();
    analysis_latias();
    std::cout << "======== End Evaluate ========" <<std::endl;
}

void Evaluator::get_mem_info(){
    // GetMemInfo pass_(*this, eva_root_);
    // pass_.run(root_);
}

// void Evaluator::analysis(){
//     SimAnalysis pass_(*this, eva_root_);
//     pass_.run(root_);
// }

void Evaluator::init_analysis(){
    InitAnalysis pass_(*this, eva_root_);
    pass_.run(root_);
}

void InitAnalysis::run(const Node* root){
    // init offset
    is_init_offset_ = true;
    root->accept(this);
    is_init_offset_ = false;
    is_get_offset_ = true;
    root->accept(this);
    is_get_offset_ = false;
    std::cout << "Offset Init Finish!" << std::endl;
    is_init_op_ = true;
    root->accept(this);
    is_init_op_ = false;
    is_init_tensor_ = true;
    root->accept(this);
    is_init_tensor_ = false;
    std::cout << "Tensor Init Finish!" << std::endl;
    printRootIOTensor();
}

void InitAnalysis::visitScope(const ScopeNode* node){
    if(is_init_offset_){ initOffset(node); return; }
    if(is_init_op_){ initOpTensor(node); return; }
    if(is_init_tensor_){ initTensor(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
}

void InitAnalysis::visitTile(const TileNode* node){
    if(is_init_offset_){ initOffset(node); return; }
    if(is_init_op_){ initOpTensor(node); return; }
    if(is_init_tensor_){ initTensor(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
}

void InitAnalysis::visitOp(const OpNode* node){
    if(is_init_offset_){ initOffset(node); return; }
    if(is_init_op_){ initOpTensor(node); return; }
    if(is_init_tensor_){ initTensor(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
}

void InitAnalysis::visitTrans(const TransNode* node){
    if(is_init_offset_){ initOffset(node); return; }
    if(is_init_op_){ initOpTensor(node); return; }
    if(is_init_tensor_){ initTensor(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
}


void InitAnalysis::initTensor(const Node* node){
    // if(node){}
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    if (current_node_->ori_node_->get_type() == Node::Op){
        op_num_ -= 1;
        auto name = nameSub(current_node_->ori_node_->get_name());
        auto tensorName = getInOutTensor(name);
        auto dimName = getDimName(tensorName);
        for (unsigned i = 0; i < tensorName.size(); i++){
            auto tensor = tensorName[i];
            auto tensor_dim = dimName[tensor];
            TensorMap tensorMap(tensor, tensor_dim);
            // output tensor
            if(i == tensorName.size() - 1){
                // current_node_->add_output_tensors(tensorMap, tensor);
                // 如果是最后一个，则直接向上遍历，逐个添加至output
                if (op_num_ == 0) addParentTensor(current_node_, tensorMap, false);
            }
            else{
                // producer存在tensor
                if (producer_map_.count(tensor)){
                    auto producer_node_ = producer_map_[tensor].second;
                    auto common_node_path_ = findCommonNode(producer_node_, current_node_);
                    // auto common_node_ = common_node_path_.common_node_;
                    auto producer_path_ = common_node_path_.producer_path_;
                    auto consumer_path_ = common_node_path_.consumer_path_;
                    addParentTensor(producer_path_, tensorMap, false);
                    addParentTensor(consumer_path_, tensorMap, true);
                }
                // 不存在tensor
                else{
                    addParentTensor(current_node_, tensorMap, true);
                }
            }
        }
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}


CommonNodePath InitAnalysis::findCommonNode(EvaNode* source, EvaNode* target){
    // CommonNodePath common_node_path;
    std::vector<EvaNode*> source_path;
    std::vector<EvaNode*> target_path;
    EvaNode* common_node = nullptr;
    auto tmp_node = source->get_parent();
    while(tmp_node != nullptr){
        source_path.push_back(tmp_node);
        tmp_node = tmp_node->get_parent();
    }
    tmp_node = target->get_parent();
    while(tmp_node != nullptr){
        target_path.push_back(tmp_node);
        tmp_node = tmp_node->get_parent();
    }
    // find common node
    unsigned i = 0;
    unsigned j = 0; 
    for (; i < source_path.size(); i++){
        j = 0;
        if (source_path[i]->ori_node_->get_type() == Node::Scope) continue;
        for (; j < target_path.size(); j++){
            if (target_path[j]->ori_node_->get_type() == Node::Scope) continue;
            if (source_path[i] == target_path[j]){
                common_node = target_path[j];
                break;
            }
        }
        if(common_node != nullptr) break;
    }
    source_path.erase(source_path.begin() + i, source_path.end());
    target_path.erase(target_path.begin() + j, target_path.end());
    
    if(common_node == nullptr){
        std::cout << "OP1: " << source->ori_node_->get_name()
                  << ", OP2: " << target->ori_node_->get_name()
                  << " have no common node found!" << std::endl;
        return CommonNodePath();
    }
    return CommonNodePath(common_node, source_path, target_path);
}

// path cannot contain common node
void InitAnalysis::addParentTensor(std::vector<EvaNode*> path, TensorMap tensorMap, bool is_input){
    for (auto node: path){
        if(is_input){
            node->add_input_tensors(tensorMap, tensorMap.tensor_name_);
        }
        else{
            node->add_output_tensors(tensorMap, tensorMap.tensor_name_);
        }
    }
}

void InitAnalysis::addParentTensor(EvaNode* source, TensorMap tensorMap, bool is_input){
    if(is_input){
        auto tmp_node = source->get_parent();
        while(tmp_node != nullptr){
            tmp_node->add_input_tensors(tensorMap, tensorMap.tensor_name_);
            tmp_node = tmp_node->get_parent();
        }
    }
    else{
        auto tmp_node = source->get_parent();
        while(tmp_node != nullptr){
            tmp_node->add_output_tensors(tensorMap, tensorMap.tensor_name_);
            tmp_node = tmp_node->get_parent();
        }   
    }
}


void InitAnalysis::printRootIOTensor(){
    std::cout << "======== Root I/O Tensor ========" <<std::endl;
    for (auto& tensor: root_->input_tensors_){
        std::cout << "Input Tensor: " << tensor.first << std::endl;
    }
    for (auto& tensor: root_->output_tensors_){
        std::cout << "Output Tensor: " << tensor.first << std::endl;
    }
    std::cout << "======== End Root I/O Tensor ========" <<std::endl;
}

void InitAnalysis::initOpTensor(const Node* node){
    
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }

    // no child -- OP Trans
    if (current_node_->ori_node_->get_type() == Node::Op){
        op_num_ += 1;
        auto name = nameSub(current_node_->ori_node_->get_name());
        auto tensorName = getInOutTensor(name);
        auto dimName = getDimName(tensorName);
        for (unsigned i = 0; i < tensorName.size(); i++){
            auto tensor = tensorName[i];
            auto tensor_dim = dimName[tensor];
            TensorMap tensorMap(tensor, tensor_dim);
            if(i == tensorName.size() - 1){
                current_node_->add_output_tensors(tensorMap, tensor);
                std::pair<TensorMap, EvaNode*> tensorMap_pair(tensorMap, current_node_);
                TILEEXP_ASSERT(producer_map_.insert({tensor, tensorMap_pair}).second, "Duplicate producer tensor");
            }
            else{
                current_node_->add_input_tensors(tensorMap, tensor);
            }
        }
    }
    //Trans -- nothing to do
    // else{}

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void InitAnalysis::initOffset(const Node* node){
    // current_node_->clear_IO_tensors();
    current_node_->initDimOffset();

    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
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


loop::TileExp::Descriptor InitAnalysis::findLoop(std::string name, std::vector<loop::TileExp::Descriptor> loops){
    // find loop
    for (auto loop: loops){
        if (loop.name_ == name) return loop;
    }
    TILEEXP_ASSERT(false, "findLoop fail");
}

std::pair<loop::TileExp::Descriptor, bool> InitAnalysis::findChildLoop(std::string name, std::vector<loop::TileExp::Descriptor> loops){
    // find loop
    for (auto loop: loops){
        if (loop.name_ == name) return std::pair<loop::TileExp::Descriptor, bool>(loop, true);
    }
    return std::pair<loop::TileExp::Descriptor, bool>(loop::TileExp::Descriptor(), false);
    // TILEEXP_ASSERT(false, "findLoop fail");
}

// offset 表示当前节点的循环计数+1时，对应dim的变化
void InitAnalysis::getOffset(const Node* node){

    // DFS
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    // for current_node_
    // std::map<std::string, int> tmp_dim_offset_;
    std::set<std::string> current_node_dim_name_;
    for (auto tmp_loop: node->loopnests_){
        current_node_dim_name_.insert(tmp_loop.name_);
    }

    if (node->get_children().size() != 0){
        if(node->get_type() == Node::Scope){
            // *** 这里我们不对scope节点进行处理，如果在计算DM时用到了scope节点，则仅需要对其子节点进行遍历找到对应offset
        }
        else{
            // 这里需要对当前节点的offset进行计算
            for (auto tmp_loop: node->loopnests_){
                bool dim_found = false;
                for (unsigned i = 0; i < node->get_children().size(); i++){
                    // auto child = node->get_children()[i];
                    auto eva_child = current_node_->get_children()[i];
                    auto pair_tmp = BFSOffsetLoop(eva_child, tmp_loop.name_);
                    // 如果在当前子节点下没找到当前dim，则到下一个
                    if (!pair_tmp.second) continue; 
                    dim_found = true;
                    auto offset_tmp = pair_tmp.first.first;
                    auto loop_tmp = pair_tmp.first.second;
                    current_node_->add_dim_offset(tmp_loop.name_, loop_tmp.first * offset_tmp.first);
                    current_node_->add_last_dim_offset(tmp_loop.name_, (loop_tmp.second - 1) * offset_tmp.first + offset_tmp.second);
                    break;
                }
                TILEEXP_ASSERT(dim_found, "Current child has no related dim found");
            }
        }
    }
    else{
        if(node->get_type() == Node::Trans){ }
        else{
            for (auto tmp_loop: node->loopnests_){
                current_node_->add_dim_offset(tmp_loop.name_, 1);
                current_node_->add_last_dim_offset(tmp_loop.name_, 1);
            }
        }
    }

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}



void Evaluator::analysis_latias(){
    PerfAnalysis pass_(*this, eva_root_);
    pass_.run(root_);
    data_movements_ = pass_.data_movements_;
}

void PerfAnalysis::run(const Node* root){
    // init(root);
    // analysis
    root->accept(this);
    std::cout << "Analysis Finish!" << std::endl;
}

void PerfAnalysis::init(const Node* root){
    // get offset
    is_get_offset_ = true;
    root->accept(this);
    is_get_offset_ = false;
    std::cout << "Offset Set Finish!" << std::endl;
}


void PerfAnalysis::visitScope(const ScopeNode* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "ScopeNode is nullptr");
    // visitScopeLoop(node);
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::visitTile(const TileNode* node){
    visitTileLoop(node, 0);
}

void PerfAnalysis::visitOp(const OpNode* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "OpNode is nullptr");
    // visitOpLoop(node);
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::visitTrans(const TransNode* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "TransNode is nullptr");
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
    // visitTransLoop();
}



// 计算data move的几个步骤
// 每一个op都会根据offset计算产生一个input和output tensor，以及每一个维度的循环带来的range，
// 作为data movement
// 每一个tile都会根据自身的tiling factor，对每一个child的io tensor进行维度变化
// 目前只考虑输入时op节点倍数的情况
void PerfAnalysis::visitOpLoop(const Node* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "OpNode is nullptr");

    // auto dim_bound = node->loopnests_.size();

    // for (unsigned i = 0; i < dim_bound; i++){ 
    //     current_loop_state_.push_back(current_node_->loopnests_[i]);
    // }

    // std::string name = nameSub(node->name_);
    // std::vector<std::string> tensorName = getInOutTensor(name);
    // std::map<std::string, std::vector<std::string> > dimName = getDimName(tensorName);

    // // 每一个循环为当前OP节点添加一个tensormap -- 需要改为input dim和output dim
    // // 无需这么麻烦，只需要每个tile都能获取到对应子节点所包含的tensor即可 -- TBD
    // // 每一个node的属性内均需要包含tensor的vector
    // DimRange dimRange;
    // std::vector<DimRange> dimRangeVec;
    // for(unsigned i = 0; i < tensorName.size(); i++){
    //     // output
    //     dimRangeVec.clear();
    //     if(i == tensorName.size() - 1){
    //         auto tensor = tensorName[i];
    //         auto tensor_dim = dimName[tensor];
    //         for(auto dim_name: tensor_dim){
    //             dimRange.dim_name_ = dim_name;
    //             dimRange.low_bound_ = 0;
    //             if(isLastLoop(dim_name)){
    //                 dimRange.high_bound_ = 0;
    //             }
    //             dimRangeVec.push_back(dimRange);
    //         }
    //         TensorMap tensorMap(tensor, tensor_dim, dimRangeVec);
    //         current_node_->output_tensors_.push_back(tensorMap);
    //     }
    //     // input
    //     else{   
    //         auto tensor = tensorName[i];
    //         auto tensor_dim = dimName[tensor];
    //         for(auto dim_name: tensor_dim){
    //             dimRange.dim_name_ = dim_name;
    //             dimRange.low_bound_ = 0;
    //             if(isLastLoop(dim_name)){
    //                 dimRange.high_bound_ = 0;
    //             }
    //             dimRangeVec.push_back(dimRange);
    //         }
    //         TensorMap tensorMap(tensor, tensor_dim, dimRangeVec);
    //         current_node_->input_tensors_.push_back(tensorMap);
    //     }
    // }

    // for (unsigned i = 0; i < dim_bound; i++){ 
    //     current_loop_state_.pop_back();
    // }
    // current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::visitScopeLoop(const Node* node){

    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::visitTransLoop(){
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

// 还没乘offset -- TBD -- done
int64_t PerfAnalysis::addCurrentTensor(bool is_input){
    int64_t data_movements_tmp = 0;
    if(is_input){
        for (auto &tensor_tmp: current_node_->input_tensors_){
            auto tensor_name = tensor_tmp.first;
            auto tensor_dims = tensor_tmp.second.tensor_dims_;
            std::unordered_map<std::string, DimRange> dim_range_map;
            for(auto &dim_name : tensor_dims){
                StEd dim_range;
                if (current_node_->current_dim_range_[dim_name].size() > 0) {
                    dim_range = current_node_->current_dim_range_[dim_name].back();
                    auto offset_tmp = current_node_->current_offset_[dim_name].back();
                    dim_range.first *= offset_tmp.first;
                    dim_range.second = (dim_range.second - 1) * offset_tmp.first + offset_tmp.second;
                }
                else {
                    auto pair_tmp = BFSOffsetLoop(current_node_, dim_name);
                    if (pair_tmp.second) {
                        auto offset_tmp = pair_tmp.first.first;
                        auto loop_tmp = pair_tmp.first.second;
                        dim_range = std::pair<int, int>(0, offset_tmp.first * (loop_tmp.second - 1) + offset_tmp.second);
                    }
                    else TILEEXP_ASSERT(false, "BFS fail");
                }
                DimRange dimRange(dim_name, dim_range);
                dim_range_map[dim_name] = dimRange;
            }
            TensorMap inputTensorMap(tensor_name, tensor_dims, dim_range_map);
            // 不对root节点进行数据搬运计算
            if(current_node_->get_parent() != nullptr){
                data_movements_tmp += TileExp::calTensorMapDM(current_node_->input_tensors_[tensor_name], inputTensorMap);
            }
            current_node_->input_tensors_[tensor_name] = inputTensorMap;
        }
    }
    else{
        for (auto &tensor_tmp: current_node_->output_tensors_){
            auto tensor_name = tensor_tmp.first;
            auto tensor_dims = tensor_tmp.second.tensor_dims_;
            std::unordered_map<std::string, DimRange> dim_range_map;
            for(auto &dim_name : tensor_dims){
                StEd dim_range;
                if (current_node_->current_dim_range_[dim_name].size() > 0) {
                    dim_range = current_node_->current_dim_range_[dim_name].back();
                    auto offset_tmp = current_node_->current_offset_[dim_name].back();
                    dim_range.first *= offset_tmp.first;
                    dim_range.second = (dim_range.second - 1) * offset_tmp.first + offset_tmp.second;
                }
                else {
                    auto pair_tmp = BFSOffsetLoop(current_node_, dim_name);
                    if (pair_tmp.second) {
                        auto offset_tmp = pair_tmp.first.first;
                        auto loop_tmp = pair_tmp.first.second;
                        dim_range = std::pair<int, int>(0, offset_tmp.first * (loop_tmp.second - 1) + offset_tmp.second);
                    }
                    else TILEEXP_ASSERT(false, "BFS fail");
                }
                DimRange dimRange(dim_name, dim_range);
                dim_range_map[dim_name] = dimRange;

            }
            TensorMap outputTensorMap(tensor_name, tensor_dims, dim_range_map);
            data_movements_tmp += TileExp::calTensorMapDM(outputTensorMap);
            current_node_->output_tensors_[tensor_name] = outputTensorMap;
        }
    }
    return data_movements_tmp;
}


bool PerfAnalysis::isFirstLoop(std::vector<int> loop_ori, std::vector<int> loop_current){
    for (unsigned i = 0; i < loop_ori.size(); i++){
        if (loop_ori[i] != loop_current[i]) return false;
    }
    return true;
}

void PerfAnalysis::visitTileLoop(const Node* node, unsigned current_dim_idx){
    
    // 包含几个维度
    auto dim_bound = node->loopnests_.size();
    
    // // 如果当前tile的几个维度已经遍历完，则进行继续的DFS访问
    // if (current_dim_idx == dim_bound){
    //     return;
    // }

    std::string dim_name_ = current_node_->loopnests_[current_dim_idx].name_;
    int ori_start = current_node_->loopnests_[current_dim_idx].start;
    int* loop_start = &(current_node_->loopnests_[current_dim_idx].start);
    bool is_last_dim;
    int loop_stride = current_node_->loopnests_[current_dim_idx].stride;
    TILEEXP_ASSERT(loop_stride == 1, "currently only support stride = 1");
    
    // 若该dim为空或者是该dim最后一个loop
    if (node->get_parent() == nullptr || isLastLoop(dim_name_)){ is_last_dim = true; }
    else{ is_last_dim = false; }
    
    int loop_end;
    // 计算当前维度的循环结束范围
    if (is_last_dim){ 
        loop_end = current_node_->loopnests_[current_dim_idx].residual_end; 
    }
    else{ 
        loop_end = current_node_->loopnests_[current_dim_idx].end; 
    }
    
    // 此处可简化加快 -- 0， 0->1，n-1->n -- 暂不实现
    // 此处需要添加当前loop的起始结束的信息，在PerfAnalysis中添加，如此可以计算出当前节点和子节点对应的范围
    for (; *loop_start < loop_end; *loop_start += loop_stride){
        if (*loop_start + loop_stride < loop_end){ vec_last_dim_[dim_name_].push_back(false); }
        else { vec_last_dim_[dim_name_].push_back(true); }
        
        std::pair<int, int> current_tile_loop_range = {ori_start, loop_end};
        StEd curr_offset_tmp = StEd(current_node_->dim_offset_[dim_name_], current_node_->last_dim_offset_[dim_name_]);
        current_node_->current_dim_range_[dim_name_].push_back(current_tile_loop_range);
        current_node_->current_offset_[dim_name_].push_back(curr_offset_tmp);
        current_node_->ori_start_.push_back(ori_start);
        current_node_->current_start_.push_back(*loop_start);
        current_node_->node_dim_bound_[dim_name_] = current_tile_loop_range;
        
        // *** 在最内层的dim中的计算当前级别的tile的IO张量信息对应的搬运量，每次都需要重新计算，并保留结果，供下次计算做覆盖
        if (current_dim_idx == dim_bound - 1 && isFirstLoop(current_node_->ori_start_, current_node_->current_start_)){
            // input
            auto input_dm = addCurrentTensor(true);
            // output
            auto output_dm = addCurrentTensor(false);
            if(input_dm != 0){
                std::cout << current_node_->ori_node_->target_level_name << ": Input Tensor Data Movement: " << input_dm << std::endl;
            }
            if(output_dm != 0){
                std::cout << current_node_->ori_node_->target_level_name << ": Output Tensor Data Movement: " << output_dm << std::endl;
            }
            data_movements_ += input_dm + output_dm;
        }

        if (current_dim_idx + 1 < dim_bound){
            // change dimension
            visitTileLoop(node, current_dim_idx + 1);
        }
        else{
            for (unsigned i = 0 ; i < node->get_children().size(); i++){
                current_node_ = current_node_->get_children()[i];
                node->get_children()[i]->accept(this);
            }
            // *** 在这里根据子节点，获取子节点整体的latency
            // AddChildTensorMap();
            // for (unsigned i = 0 ; i < node->get_children().size(); i++){
            //     current_node_ = current_node_->get_children()[i];
            //     auto current_node_type = current_node_->ori_node_->get_type();
            //     if (current_node_type == Node::Scope){
            //         node->get_children()[i]->accept(this);
            //         // AddLatency(); -- TBD
            //     }
            //     else if (current_node_type == Node::Op){
            //         // 获取子节点的io tensor
            //         auto& tensor_map_in = current_node_->input_tensors_;
            //         auto& tensor_map_out = current_node_->output_tensors_;
            //         // 根据current_dim_range_计算当前tensor的范围
            //         auto data_move = CalDataMove(tensor_map_in, tensor_map_out, node->loopnests_);
            //         AddDataMove(data_move);
            //         auto flops = CalFlops(tensor_map_in, tensor_map_out);
            //         // struct Latency -- int, int, int
            //         // 先计算搬运量带来的延迟，后根据interconnection计算 copyin, process, copyout的情况
            //         auto Latency = GetLatency(data_move, flops);
            //         // 前面需要一个child_latency的集合
            //         AddLatency(Latency);
            //         // 复原current_node_到当前节点
            //         current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
            //     }
            //     else if (current_node_type == Node::Tile){
            //         // 先遍历该tile的全部子节点
            //         node->get_children()[i]->accept(this);
            //         // 再计算该节点的分析
            //         auto tensor_map_in = &(current_node_->input_tensors_);
            //         auto tensor_map_out = &(current_node_->output_tensors_);
            //         // auto dim_range = GetDimRange(); // --TBD -- std::vector<std::string, std::pair<int, int>>
            //         auto data_move = CalDataMove(tensor_map_in, tensor_map_out, node->loopnests_);
            //         AddDataMove(data_move);
            //         auto flops = CalFlops(tensor_map_in, tensor_map_out);
            //         AddLatency(data_move, flops);
            //     }
            //     else if (current_node_type == Node::Trans){
            //         node->get_children()[i]->accept(this);
            //     }
            // }
        }
        vec_last_dim_[dim_name_].pop_back();
        current_node_->current_offset_[dim_name_].pop_back();
        current_node_->current_dim_range_[dim_name_].pop_back();
        current_node_->ori_start_.pop_back();
        current_node_->current_start_.pop_back();
        // current_loop_state_.pop_back();
    }
    
    if (current_dim_idx == 0){
        current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
    }
    // 还原初始start
    *loop_start = ori_start;
}


bool PerfAnalysis::isLastLoop(std::string dim_name){
    bool tmp = true;
    auto vec_last_loop = vec_last_dim_[dim_name];
    for(auto last_loop: vec_last_loop){
        tmp = tmp && last_loop;
    }
    return tmp;
}


std::vector<std::string> PerfAnalysis::getInOutTensor(std::string name){
    std::vector<std::string> res;
    auto workload = evaluator_.workloads_.get_workload(name);
    for (auto& dataSpaceName: workload->get_ins()){
        res.push_back(dataSpaceName);
    }
    res.push_back(workload->get_out());
    return res;
}

// last dimName is the output
std::map<std::string, std::vector<std::string> > PerfAnalysis::getDimName(std::vector<std::string> tensorName){
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


std::vector<std::string> InitAnalysis::getInOutTensor(std::string name){
    std::vector<std::string> res;
    auto workload = evaluator_.workloads_.get_workload(name);
    for (auto& dataSpaceName: workload->get_ins()){
        res.push_back(dataSpaceName);
    }
    res.push_back(workload->get_out());
    return res;
}

// last dimName is the output
std::map<std::string, std::vector<std::string> > InitAnalysis::getDimName(std::vector<std::string> tensorName){
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

// 递归循环，用于表示不同loop dimension
void PerfAnalysis::RecursiveLoop(const Node* node, std::vector<loop::TileExp::Descriptor> loop_vector, unsigned current_dim_idx, unsigned last_dim_idx, bool is_last_loop){
    if (last_dim_idx == current_dim_idx){
        // 递归循环
        return;
        // RecursiveLoop(node, current_loop_idx + 1, last_loop_idx);
    }
    else{
        // go to child-node
        auto dim = loop_vector[current_dim_idx];

        for (int i = dim.start; i < dim.end; i += dim.stride){
            // update loop state
            // current_loop_state_.push_back(dim);
            // dim_offset_all_[dim.name_].push_back(current_node_->dim_offset_[dim.name_]);

            // // update loop state
            // current_node_->loopnests_[current_loop_idx].start = i;
            // current_node_->loopnests_[current_loop_idx].end = i + dim.stride;
            // current_node_->loopnests_[current_loop_idx].residual_end = 0;

            // recursive loop
            RecursiveLoop(node, loop_vector, current_dim_idx + 1, last_dim_idx, is_last_loop);

            // // reset loop state
            // current_node_->loopnests_[current_loop_idx].start = dim.start;
            // current_node_->loopnests_[current_loop_idx].end = dim.end;
            // current_node_->loopnests_[current_loop_idx].residual_end = dim.residual_end;
        }
    }
}



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
        // dim_results[dim_name] = tem_result;
        // unsigned loop_upbound = dim_residual_ends[dim_name].size();
        // for (unsigned i = 0; i < loop_upbound; ++i){
        //     tem_result = dim_residual_ends[dim_name][i];
        //     for (unsigned j = i + 1; j < loop_upbound; j++){
        //         tem_result *= dim_ends[dim_name][j];
        //     }
        //     dim_results[dim_name] += tem_result;
        // } 
    }
    return dim_results;
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


} // namespace Analysis
} // namespace TileExp