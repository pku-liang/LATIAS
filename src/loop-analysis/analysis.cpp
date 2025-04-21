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
    latency_ = pass_.current_node_->process_latency_;
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
    current_node_->latency_sub_vec_.clear();
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
        auto child = current_node_->get_children()[i];
        if (child->ori_node_->get_type() == Node::Trans) continue;
        Latency latency(child->input_latency_, child->output_latency_, child->process_latency_);
        current_node_->latency_sub_vec_.push_back(latency);
        current_node_->sub_latency_num_ += 1;
    }
    
    current_node_->latency_sub_vec_[0].sub_latency_num_ = current_node_->sub_latency_num_;
    current_node_->latency_vec_.push_back(current_node_->latency_sub_vec_);
    
    // 处理子tile的latency

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::visitTile(const TileNode* node){
    visitTileLoop(node, 0);
}

// 
void PerfAnalysis::visitOp(const OpNode* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "OpNode is nullptr");
    // int64_t flops_tmp = 1;
    std::unordered_map<std::string, int> dim_end;
    
    for (auto & loop: current_node_->loopnests_){
        dim_end[loop.name_] = loop.end;
    }

    int64_t input_dm = 0;
    for (auto &tensor: current_node_->input_tensors_){
        auto tensor_name = tensor.first;
        auto tensor_dims = tensor.second.tensor_dims_;
        std::vector<int> tensor_data_movements;
        for (auto &dim_name: tensor_dims){
            tensor_data_movements.push_back(dim_end[dim_name]);
        }
        int64_t tmp = 1;
        for (auto &dim: tensor_data_movements){
            tmp *= dim;
        }
        input_dm += tmp;
    }
    int64_t output_dm = 0;
    for (auto &tensor: current_node_->output_tensors_){
        auto tensor_name = tensor.first;
        auto tensor_dims = tensor.second.tensor_dims_;
        std::vector<int> tensor_data_movements;
        for (auto &dim_name: tensor_dims){
            tensor_data_movements.push_back(dim_end[dim_name]);
        }
        int64_t tmp = 1;
        for (auto &dim: tensor_data_movements){
            tmp *= dim;
        }
        output_dm += tmp;
    }
    data_movements_ += output_dm + input_dm;

    // start latency
    auto target_level = current_node_->ori_node_->get_target_level_name();
    auto hardware_specs = evaluator_.arch_topo_.archTopo_map_;
    current_node_->process_latency_ = hardware_specs[target_level].compute_cycles;
    current_node_->input_latency_ = getInputLatency(current_node_, input_dm);
    current_node_->output_latency_ = getOutputLatency(current_node_, output_dm);
    // std::cout << current_node_->process_latency << std::endl;
    // recover current_node_
    
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::visitTrans(const TransNode* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "TransNode is nullptr");
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

std::string PerfAnalysis::findLastOpTrans(EvaNode* node){
    // auto num = node->get_children().size();
    EvaNode tmp_node = *node;
    while (true){
        auto num = tmp_node.get_children().size();
        if (tmp_node.ori_node_->get_type() == Node::Op || tmp_node.ori_node_->get_type() == Node::Trans) break;
        // tmp_node = tmp_node->get_children()[num - 1];
        if (tmp_node.get_children()[num - 1]->ori_node_->get_type() == Node::Trans) tmp_node = *tmp_node.get_children()[num - 1];
        else tmp_node = *tmp_node.get_children()[num - 1];
    }
    TILEEXP_ASSERT(tmp_node.ori_node_->get_type() == Node::Op || tmp_node.ori_node_->get_type() == Node::Trans, "Source is not Op node");
    return tmp_node.ori_node_->get_target_level_name();
}

int PerfAnalysis::getOutputLatency(EvaNode* node, int64_t data_movement){
    if (node->ori_node_->get_type() == Node::Scope || node->ori_node_->get_type() == Node::Trans) return 0;

    std::string source_level; 
    if (node->ori_node_->dataflow_mode_ != Node::Forward){
        source_level = current_node_->ori_node_->get_target_level_name();
    }
    else{
        // EvaNode* tmp_node = node;
        // auto targetname = findLastOp(node);
        source_level = findLastOpTrans(node);
    }
    if (node->get_parent() == nullptr) return 0;
    // int parent_child_num = node->get_parent()->get_children().size();
    // int current_node_position = findCurrentNodePosition(node);
    auto target_level = findTarget(node);
    auto connection = source_level + "2" + target_level.first;
    auto intercon = evaluator_.intercon_.intercon_attri_map_;
    auto bandwidth = intercon[connection].write_bandwidth_;
    return int((data_movement + bandwidth - 1) / bandwidth);
}

std::pair<std::string, bool> PerfAnalysis::findTarget(EvaNode* node){
    if (node->ori_node_->dataflow_mode_ == Node::Forward){
        while(true){
            int parent_child_num = node->get_parent()->get_children().size();
            int current_node_position = findCurrentNodePosition(node);
            if (current_node_position + 1 == parent_child_num){
                node = node->get_parent();
                continue;
            }
            auto target_node = node->get_parent()->get_children()[current_node_position + 1];
            TILEEXP_ASSERT(target_node->ori_node_->get_type() == Node::Trans, "findTarget fail");
            return std::pair<std::string, bool>(target_node->ori_node_->get_target_level_name(), true);
    
        }
    }
    else{
        auto parent_node = node->get_parent();
        while(parent_node->ori_node_->get_type() != Node::Tile) parent_node = parent_node->get_parent();
        return std::pair<std::string, bool>(parent_node->ori_node_->get_target_level_name(), false);
    }
}

// 获取当前节点输入的latency
int PerfAnalysis::getInputLatency(EvaNode* node, int64_t data_movement){
    // 不计算scope和trans的latency
    if (node->ori_node_->get_type() == Node::Scope || node->ori_node_->get_type() == Node::Trans) return 0;

    auto target_level = current_node_->ori_node_->get_target_level_name();
    if (node->get_parent() == nullptr) return 0;
    // int parent_child_num = node->get_parent()->get_children().size();
    int current_node_position = findCurrentNodePosition(node);
    auto source_level = findSource(node, current_node_position);
    auto connection = source_level.first + "2" + target_level;
    auto intercon = evaluator_.intercon_.intercon_attri_map_;
    auto bandwidth = intercon[connection].read_bandwidth_;
    return int((data_movement + bandwidth - 1) / bandwidth);
}

// 找到当前节点的上一个平级节点的最深处节点是否是trans节点 x
// 找到当前节点数据的来源
std::pair<std::string, bool> PerfAnalysis::findSource(EvaNode* node, int current_node_position){
    // 上一个节点不存在
    if (node->ori_node_->dataflow_mode_ == Node::Forward){
        if (node->get_parent()->get_children().size() == 1 || current_node_position == 0){
            auto parent_node = node->get_parent();
            while(parent_node->ori_node_->get_type() != Node::Tile) parent_node = parent_node->get_parent();
            return std::pair<std::string, bool>(parent_node->ori_node_->get_target_level_name(), false);
        } 
        // return std::pair<std::string, bool>("", false);
        // 上一个节点存在
        auto former_node = node->get_parent()->get_children()[current_node_position - 1];
        if (former_node->ori_node_->get_type() == Node::Trans){
            former_node = node->get_parent()->get_children()[current_node_position - 2];
            while(true){
                auto num = former_node->get_children().size();
                if (num > 0) former_node = former_node->get_children()[num - 1];
                else break;
            }
            TILEEXP_ASSERT(former_node->ori_node_->get_type() == Node::Op, "Source is not Op node");
            return std::pair<std::string, bool>(former_node->ori_node_->get_target_level_name(), true);
        }
        else {
            auto parent_node = node->get_parent();
            while(parent_node->ori_node_->get_type() != Node::Tile) parent_node = parent_node->get_parent();
            return std::pair<std::string, bool>(parent_node->ori_node_->get_target_level_name(), false);
        }
    }
    else{
        auto parent_node = node->get_parent();
        while(parent_node->ori_node_->get_type() != Node::Tile) parent_node = parent_node->get_parent();
        return std::pair<std::string, bool>(parent_node->ori_node_->get_target_level_name(), false);
    }

}

// 找到当前节点在父节点的子节点中的位置
int PerfAnalysis::findCurrentNodePosition(EvaNode* node){
    EvaNode* tmp_node;
    if (node->get_parent() == nullptr) return 0;
    tmp_node = node->get_parent();

    for (unsigned i = 0; i < tmp_node->get_children().size(); i++){
        if (node == tmp_node->get_children()[i]) return i;
    }
    
    TILEEXP_ASSERT(false, "findCurrentNodePosition fail");
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
                auto skew = dim_skew[dim_name];
                StEd dim_range;
                if (current_node_->current_dim_range_[dim_name].size() > 0) {
                    dim_range = current_node_->current_dim_range_[dim_name].back();
                    auto offset_tmp = current_node_->current_offset_[dim_name].back();
                    dim_range.first = dim_range.first * offset_tmp.first + skew;
                    dim_range.second = (dim_range.second - 1) * offset_tmp.first + offset_tmp.second + skew;
                }
                else {
                    auto pair_tmp = BFSOffsetLoop(current_node_, dim_name);
                    if (pair_tmp.second) {
                        auto offset_tmp = pair_tmp.first.first;
                        auto loop_tmp = pair_tmp.first.second;
                        dim_range = std::pair<int, int>(skew, skew + offset_tmp.first * (loop_tmp.second - 1) + offset_tmp.second);
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
                auto skew = dim_skew[dim_name];
                StEd dim_range;
                if (current_node_->current_dim_range_[dim_name].size() > 0) {
                    dim_range = current_node_->current_dim_range_[dim_name].back();
                    auto offset_tmp = current_node_->current_offset_[dim_name].back();
                    dim_range.first = dim_range.first * offset_tmp.first + skew;
                    dim_range.second = skew + (dim_range.second - 1) * offset_tmp.first + offset_tmp.second;
                }
                else {
                    auto pair_tmp = BFSOffsetLoop(current_node_, dim_name);
                    if (pair_tmp.second) {
                        auto offset_tmp = pair_tmp.first.first;
                        auto loop_tmp = pair_tmp.first.second;
                        dim_range = std::pair<int, int>(skew, skew + offset_tmp.first * (loop_tmp.second - 1) + offset_tmp.second);
                    }
                    else TILEEXP_ASSERT(false, "BFS fail");
                }
                DimRange dimRange(dim_name, dim_range);
                dim_range_map[dim_name] = dimRange;

            }
            TensorMap outputTensorMap(tensor_name, tensor_dims, dim_range_map);
            if(current_node_->get_parent() != nullptr){
                data_movements_tmp += TileExp::calTensorMapDM(outputTensorMap);
            }
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
    
    current_node_->current_dim_skew_[dim_name_] = 0;
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
        // current_node_->node_dim_bound_[dim_name_] = current_tile_loop_range;
        
        // *** 在最内层的dim中的计算当前级别的tile的IO张量信息对应的搬运量，每次都需要重新计算，并保留结果，供下次计算做覆盖
        if (current_dim_idx == dim_bound - 1 && isFirstLoop(current_node_->ori_start_, current_node_->current_start_)){
            // *** 计算data movement
            // input
            auto input_dm = addCurrentTensor(true);
            // output
            auto output_dm = addCurrentTensor(false);
            // Print
            if (false){
                std::cout << current_node_->ori_node_->target_level_name << ": Input Tensor Data Movement: " << input_dm << std::endl;
                std::cout << current_node_->ori_node_->target_level_name << ": Output Tensor Data Movement: " << output_dm << std::endl;
            }
            data_movements_ += input_dm + output_dm;
            // *** 计算latency

            // auto target_level = current_node_->ori_node_->get_target_level_name();
            // auto hardware_specs = evaluator_.arch_topo_.archTopo_map_;
            // current_node_->process_latency_ = hardware_specs[target_level].compute_cycles;
            if (current_node_->get_parent() != nullptr){
                // auto target_level = current_node_->ori_node_->get_target_level_name();
                // auto hardware_specs = evaluator_.arch_topo_.archTopo_map_;
                current_node_->input_latency_ = getInputLatency(current_node_, input_dm);
                current_node_->output_latency_ = getOutputLatency(current_node_, output_dm);
            }
            else{
                current_node_->input_latency_ = 0;
                current_node_->output_latency_ = 0;
            }
            // 清空当前节点的Latency vector
            current_node_->latency_vec_.clear();
            current_node_->sub_latency_num_ = 0;
        }

        if (current_dim_idx + 1 < dim_bound){
            // change dimension
            visitTileLoop(node, current_dim_idx + 1);
        }
        else{
            // 计算data movement
            current_node_->latency_sub_vec_.clear();
            for (unsigned i = 0 ; i < node->get_children().size(); i++){
                current_node_ = current_node_->get_children()[i];
                node->get_children()[i]->accept(this);
                auto child = current_node_->get_children()[i];
                if (child->ori_node_->get_type() == Node::Trans) continue;
                if (child->ori_node_->get_type() == Node::Scope){
                    current_node_->latency_sub_vec_ = child->latency_sub_vec_;
                    continue;
                }
                Latency latency(child->input_latency_, child->output_latency_, child->process_latency_);
                current_node_->latency_sub_vec_.push_back(latency);
                current_node_->sub_latency_num_ += 1;
            }
            current_node_->latency_sub_vec_[0].sub_latency_num_ = current_node_->sub_latency_num_;
            current_node_->latency_vec_.push_back(current_node_->latency_sub_vec_);
        }
        vec_last_dim_[dim_name_].pop_back();
        current_node_->current_offset_[dim_name_].pop_back();
        current_node_->current_dim_range_[dim_name_].pop_back();
        current_node_->ori_start_.pop_back();
        current_node_->current_start_.pop_back();
        current_node_->current_dim_skew_[dim_name_] += current_node_->dim_offset_[dim_name_];
        dim_skew[dim_name_] += current_node_->dim_offset_[dim_name_];
        // current_loop_state_.pop_back();
    }
    
    dim_skew[dim_name_] -= current_node_->current_dim_skew_[dim_name_];

    
    if (current_dim_idx == 0){
        // 收集完全部子tile的latency，计算当前tile的总process latency
        auto process_latency = computeLatency(current_node_->latency_vec_);
        if(process_latency != 0){
            current_node_->process_latency_ = process_latency;
        }
        current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
    }
    // 还原初始start
    *loop_start = ori_start;
}

// only for tile node
int64_t PerfAnalysis::computeLatency(std::vector<std::vector<Latency> > latency_vec){
    TILEEXP_ASSERT(latency_vec.size() != 0, "latency_vec is empty");
    auto latency_num = latency_vec.size();
    auto sub_latency_num = latency_vec[0].size();
    
    int64_t process_latency = 0;
    Latency total_latency;
    bool is_temporal = static_cast<const TileNode*>(current_node_->ori_node_)->get_tile_type() == TileNode::Temporal? true : false;
    bool is_forward = current_node_->ori_node_->dataflow_mode_ == Node::Forward? true : false;
    
    // 这里目前是简化的写法，只针对单gemm、vec和融合vector，不能适应复杂情况
    // for gemm
    if (is_forward){
        if (is_temporal){
            for (unsigned i = 0; i < latency_num; i++){
                auto latency = latency_vec[i];
                for (unsigned j = 0; j < sub_latency_num; j++){
                    total_latency.input_latency_ += latency[j].input_latency_;
                    total_latency.output_latency_ += latency[j].output_latency_;
                    total_latency.process_latency_ += latency[j].process_latency_;
                    if (is_print_){
                        std::cout << "Input OP: " << latency[j].input_latency_ << std::endl;
                        std::cout << "Output OP: " << latency[j].output_latency_ << std::endl;
                        std::cout << "Process OP: " << latency[j].process_latency_ << std::endl;
                        is_print_ = false;
                    }
                }

            }
        }
        //spatial
        else{
            unsigned fanout = 20;
            auto intercon = evaluator_.intercon_.intercon_attri_map_;
            auto con_type = intercon["MainMemory2L1"].con_type_;
            for (unsigned i = 0; i < fanout; i++){
                Latency tmp_latency;
                for (unsigned j = 0; j < (latency_num + fanout - 1) / fanout; j++){
                    if (j * fanout + i >= latency_num) break;
                    auto latency = latency_vec[j * fanout + i];
                    for (unsigned k = 0; k < sub_latency_num; k++){
                        if(con_type == Hardware::InterConnection::BC){
                            tmp_latency.input_latency_ += latency[k].input_latency_;
                            tmp_latency.output_latency_ += latency[k].output_latency_;
                        }
                        else{
                            tmp_latency.input_latency_ += latency[k].input_latency_ + latency[k].output_latency_;
                            tmp_latency.output_latency_ += latency[k].input_latency_ + latency[k].output_latency_;
                        }
                        tmp_latency.process_latency_ += latency[k].process_latency_;
                    }
                }
                int64_t total_latency_all = total_latency.input_latency_ + total_latency.output_latency_ + total_latency.process_latency_;
                int64_t tmp_latency_all = tmp_latency.input_latency_ + tmp_latency.output_latency_ + tmp_latency.process_latency_;
                if (total_latency_all < tmp_latency_all) total_latency = tmp_latency;
            }
            // total_latency.input_latency_ = total_latency.input_latency_ * (latency_num + fanout - 1) / fanout;
            // total_latency.output_latency_ = total_latency.output_latency_ * (latency_num + fanout - 1) / fanout;
            // total_latency.process_latency_ = total_latency.process_latency_ * (latency_num + fanout - 1) / fanout;
        }
    }
    // WB for vec and vec fusion, only consider pipeline
    else{
        if (is_temporal){
            for (unsigned i = 0; i < latency_num; i++){
                auto latency = latency_vec[i];
                for (unsigned j = 0; j < sub_latency_num; j++){
                    auto tmp = latency[j].input_latency_ + latency[j].output_latency_ + latency[j].process_latency_;
                    total_latency.input_latency_ += tmp;
                    total_latency.output_latency_ += tmp;
                    total_latency.process_latency_ += tmp;
                }
            }
        }
        else{
            auto intercon = evaluator_.intercon_.intercon_attri_map_;
            // auto bandwidth = intercon["UB2MainMemory"].read_bandwidth_;
            auto con_type = intercon["UB2MainMemory"].con_type_;
            // if (con_type == Hardware::InterConnection::FD){
            // }
            unsigned fanout = 40;
            for (unsigned i = 0; i < fanout; i++){
                Latency tmp_latency;
                for (unsigned j = 0; j < (latency_num + fanout - 1) / fanout; j++){
                    if (j * fanout + i >= latency_num) break;
                    auto latency = latency_vec[j * fanout + i];
                    for (unsigned k = 0; k < sub_latency_num; k++){
                        if(con_type == Hardware::InterConnection::FD || con_type == Hardware::InterConnection::BC){
                            tmp_latency.input_latency_ += latency[k].input_latency_;
                            tmp_latency.output_latency_ += latency[k].output_latency_;
                        }
                        else{
                            tmp_latency.input_latency_ += latency[k].input_latency_ + latency[k].output_latency_;
                            tmp_latency.output_latency_ += latency[k].input_latency_ + latency[k].output_latency_;
                        }
                        tmp_latency.process_latency_ += latency[k].process_latency_;
                    }
                }
                int64_t total_latency_all = total_latency.input_latency_ + total_latency.output_latency_ + total_latency.process_latency_;
                int64_t tmp_latency_all = tmp_latency.input_latency_ + tmp_latency.output_latency_ + tmp_latency.process_latency_;
                if (total_latency_all < tmp_latency_all) total_latency = tmp_latency;
            }
            // total_latency.input_latency_ = total_latency.input_latency_ * (latency_num + fanout - 1) / fanout;
            // total_latency.output_latency_ = total_latency.output_latency_ * (latency_num + fanout - 1) / fanout;
            // total_latency.process_latency_ = total_latency.process_latency_ * (latency_num + fanout - 1) / fanout;
        }
    }

    process_latency = std::max(std::max(total_latency.input_latency_, total_latency.output_latency_), total_latency.process_latency_);

    return process_latency;

    // for (unsigned i = 0; i < current_node_->get_children().size(), i++){
    //     auto child_node = current_node_->get_children()[i];
    //     if (child_node->ori_node_->get_type() == Node::Trans) continue;
    //     if (is_forward || )
    // }

    // ScopeType scope_type;
    // if (current_node_->get_children().size() == 1) {
    //     if (current_node_->get_children()[0]->ori_node_->get_type() == Node::Scope) scope_type = current_node_->get_children()[0]->ori_node_->scope_type_;
    //     else scope_type = ScopeType::Sequential;
    // }
    // else scope_type = ScopeType::Pipeline;

    // // temporal
    // if(is_temporal){

    // }
    // // spatial
    // else{

    // }

    // ScopeType scope_type = current_node_->ori_node_->scope_type_;

    // 对于interconnection的属性，修改input output latency

    // 对于scope，计算latency
    
    // return 0;
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