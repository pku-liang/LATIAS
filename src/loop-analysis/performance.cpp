#include "TileExp/loop-analysis/analysis.hpp"   
#include "TileExp/mapping/mapping.hpp"

namespace TileExp{

namespace Analysis{

// ************************ analysis part ************************* //

void PerfAnalysis::run(const Node* root){
    root->accept(this);
    std::cout << "Analysis Finish!" << std::endl;
}

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
    // current_node_->input_latency_ = getInputLatency(current_node_, input_dm);
    // current_node_->output_latency_ = getOutputLatency(current_node_, output_dm);
    // 此处认为op节点的cycle是完全通过compute cycle实现的，不考虑搬入搬出的latency，都囊括在process_latency_中
    current_node_->input_latency_ = 0;
    current_node_->output_latency_ = 0;

    // recover current_node_
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

// transnode也需要参与latency计算
void PerfAnalysis::visitTrans(const TransNode* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "TransNode is nullptr");
    current_node_->input_latency_ = 0;
    current_node_->process_latency_ = 0;
    current_node_->output_latency_ = 0;
    auto current_idx = std::find(leaf_vec_.begin(), leaf_vec_.end(), current_node_);
    // 若上一个叶子节点不是trans，则传输时间囊括在op节点的process_latency_中
    // 若是，则需要计算
    if ((*(current_idx - 1))->ori_node_->get_type() == Node::Trans){
        // calculate bandwidth
        auto source_level = (*(current_idx - 1))->ori_node_->get_target_level_name();
        auto target_level = current_node_->ori_node_->get_target_level_name();
        auto connection = source_level + "2" + target_level;
        auto intercon = evaluator_.intercon_.intercon_attri_map_;
        auto bandwidth = intercon[connection].bandwidth_;
        auto data_movement = 0; // TBD -- 暂不考虑连续多个trans -- 要改动的数据结构较多
        // auto data_movement = addCurrentTensor(false); // TBD

        current_node_->output_latency_ = data_movement / bandwidth; 
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::visitScope(const ScopeNode* node){
    if (node == nullptr) TILEEXP_ASSERT(false, "ScopeNode is nullptr");
    
    // scope node only visit once;
    current_node_->latency_sub_vec_.clear();
    for (unsigned i = 0; i < node->get_children().size(); i++){
        // 计算子节点的延迟
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
        // now the current_node is the scope node
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
    if (is_last_dim){ loop_end = current_node_->loopnests_[current_dim_idx].residual_end; }
    else{ loop_end = current_node_->loopnests_[current_dim_idx].end; }
    
    current_node_->current_dim_skew_[dim_name_] = 0;
    // 此处可简化加快 -- 0， 0->1，n-1->n -- 暂不实现 -- TBD
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
        
        // *** 在最内层的dim中的计算当前级别的tile的IO张量信息对应的搬运量，每次都需要重新计算，并保留结果，供下次计算做覆盖
        // 此处的firstloop的原因是我们仅在所有的循环均是最初的情况下，只计算这一次数据搬运--表示整个tile的对外输入输出搬运量
        if (current_dim_idx == dim_bound - 1 && isFirstLoop(current_node_->ori_start_, current_node_->current_start_)){
            // *** 计算data movement 并对当前tile添加留存的tensor，用于下一次添加时计算可reuse的搬运
            // input
            auto input_dm = addCurrentTensor(true);
            // output
            auto output_dm = addCurrentTensor(false);
            // Print
            if (is_print_ == 2){
                std::cout << current_node_->ori_node_->target_level_name << ": Input Tensor Data Movement: " << input_dm.first + input_dm.second << std::endl;
                std::cout << current_node_->ori_node_->target_level_name << ": Output Tensor Data Movement: " << output_dm.first + output_dm.second << std::endl;
            }
            data_movements_ += input_dm.first + input_dm.second + output_dm.first + output_dm.second;
            // *** 计算latency

            if (current_node_->get_parent() != nullptr){
                current_node_->input_latency_ = getInputLatency(current_node_, input_dm.first, input_dm.second);
                // tile的output不存在来自于trans的情况
                current_node_->output_latency_ = getOutputLatency(current_node_, output_dm.first);
            }
            else{
                current_node_->input_latency_ = 0;
                current_node_->output_latency_ = 0;
            }
            // 首个清空当前节点的Latency vector
            current_node_->latency_vec_.clear();
            current_node_->sub_latency_num_ = 0;
        }

        if (current_dim_idx + 1 < dim_bound){
            // change dimension
            visitTileLoop(node, current_dim_idx + 1);
        }
        else{
            // 计算子tile的latency
            current_node_->latency_sub_vec_.clear();
            for (unsigned i = 0 ; i < node->get_children().size(); i++){
                current_node_ = current_node_->get_children()[i];
                node->get_children()[i]->accept(this);
                auto child = current_node_->get_children()[i];
                if (child->ori_node_->get_type() == Node::Scope){
                    current_node_->latency_sub_vec_ = child->latency_sub_vec_;
                    current_node_->sub_latency_num_ += 1;
                    break; // only has one scope
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

    // 恢复偏移量
    dim_skew[dim_name_] -= current_node_->current_dim_skew_[dim_name_];

    
    if (current_dim_idx == 0){
        // 收集完全部子tile的latency，计算当前tile的总process latency（非整个tile的latency）
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
    
    // 这里目前是简化的写法，只针对单gemm、vec和融合vector，不能适应复杂情况 -- TBD
    // for gemm
    if (is_forward){
        // forward 模式默认流水线
        if (is_temporal){
            for (unsigned i = 0; i < latency_num; i++){
                auto latency = latency_vec[i];
                for (unsigned j = 0; j < sub_latency_num; j++){
                    total_latency.input_latency_ += latency[j].input_latency_;
                    total_latency.output_latency_ += latency[j].output_latency_;
                    total_latency.process_latency_ += latency[j].process_latency_;
                    if (is_print_ == 3){
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
        }
    }

    process_latency = std::max(std::max(total_latency.input_latency_, total_latency.output_latency_), total_latency.process_latency_);

    return process_latency;
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
    auto target_level = findTarget(node);
    auto connection = source_level + "2" + target_level.first;
    auto intercon = evaluator_.intercon_.intercon_attri_map_;
    auto bandwidth = intercon[connection].bandwidth_;

    // 此处可以用搬运指令效率来做替换
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
int PerfAnalysis::getInputLatency(EvaNode* node, int64_t dm_tile, int64_t dm_trans){

    int latency = 0;
    auto target_level = current_node_->ori_node_->get_target_level_name();

    // 不计算scope的latency
    if (node->ori_node_->get_type() == Node::Scope) return 0;

    for (; dm_trans > 0;) {
        if (node->get_parent() == nullptr) break;
        int current_node_position = findCurrentNodePosition(node);
        auto source_level = findSource(node, current_node_position);
        auto connection = source_level.first + "2" + target_level;
        auto intercon = evaluator_.intercon_.intercon_attri_map_;
        auto bandwidth = intercon[connection].bandwidth_;
        // 此处可以用搬运指令效率来做替换
        latency += int((dm_trans + bandwidth - 1) / bandwidth);
        break;
    }

    for (; dm_tile > 0;) {
        if (node->get_parent() == nullptr) break;
        int current_node_position = findCurrentNodePosition(node);
        auto source_level = findSource(node, current_node_position);
        auto connection = source_level.first + "2" + target_level;
        auto intercon = evaluator_.intercon_.intercon_attri_map_;
        if (target_level == source_level.first) break; // 同层级没有延迟开销
        auto bandwidth = intercon[connection].bandwidth_;
        // 此处可以用搬运指令效率来做替换
        latency += int((dm_tile + bandwidth - 1) / bandwidth);
        break;
    }

    // if (bandwidth == 0) return 0;
    return latency;
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

EvaNode* PerfAnalysis::findFirstLeaf(EvaNode* node){
    if (node->get_children().size() == 0) return node;
    else{
        for (unsigned i = 0; i < node->get_children().size(); i++){
            auto tmp_node = findFirstLeaf(node->get_children()[i]);
            if (tmp_node != nullptr) return tmp_node;
        }
    }
    return nullptr;
}


// 添加tensor并统计数据搬运，<源自tile的搬运量，源自trans的搬运量>
std::pair<int64_t, int64_t> PerfAnalysis::addCurrentTensor(bool is_input){
// int64_t PerfAnalysis::addCurrentTensor(bool is_input){
    
    int64_t dm_trans = 0;
    int64_t dm_tile = 0;

    std::string trans_tensor_name;

    auto leaf_node_tmp = findFirstLeaf(current_node_);
    auto current_leaf_idx = find(leaf_vec_.begin(), leaf_vec_.end(), leaf_node_tmp);
    // 非首个
    if (current_leaf_idx != leaf_vec_.begin()) {
        auto prev_leaf = *(current_leaf_idx - 1);
        TILEEXP_ASSERT(prev_leaf != nullptr, "find previous leaf fail");
        if (prev_leaf->ori_node_->get_type() == Node::Trans) {
            // trans仅有一个output 
            auto it = prev_leaf->output_tensors_.begin();
            trans_tensor_name = it->first;
        }
    }

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
                if (tensor_name == trans_tensor_name) dm_trans += TileExp::calTensorMapDM(current_node_->input_tensors_[tensor_name], inputTensorMap);
                else dm_tile += TileExp::calTensorMapDM(current_node_->input_tensors_[tensor_name], inputTensorMap);
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
                dm_tile += TileExp::calTensorMapDM(outputTensorMap);
            }
            current_node_->output_tensors_[tensor_name] = outputTensorMap;
        }
    }
    return std::pair<int64_t, int64_t>(dm_tile, dm_trans);
}


bool PerfAnalysis::isFirstLoop(std::vector<int> loop_ori, std::vector<int> loop_current){
    for (unsigned i = 0; i < loop_ori.size(); i++){
        if (loop_ori[i] != loop_current[i]) return false;
    }
    return true;
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



} // namespace Analysis
} // namespace TileExp