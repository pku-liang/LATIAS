#include "TileExp/loop-analysis/analysis.hpp"   
#include "TileExp/mapping/mapping.hpp"



namespace TileExp{

namespace Analysis{

std::string NameSub(std::string input){
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

// void Evaluator::analysis(){
//     SimAnalysis pass_(*this, eva_root_);
//     pass_.run(root_);
// }

void Evaluator::analysis_latias(){
    PerfAnalysis pass_(*this, eva_root_);
    pass_.run(root_);
}

void PerfAnalysis::run(const Node* root){
    // init offset
    is_init_ = true;
    root->accept(this);
    is_init_ = false;
    std::cout << "Offset Init Finish!" << std::endl;
    // get offset
    is_get_offset_ = true;
    root->accept(this);
    is_get_offset_ = false;
    std::cout << "Offset Set Finish!" << std::endl;
    // analysis
    root->accept(this);
}


void PerfAnalysis::visitScope(const ScopeNode* node){
    if(is_init_){ init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitScopeLoop(node);
}

void PerfAnalysis::visitTile(const TileNode* node){
    if(is_init_){ init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitTileLoop(node, 0);
}

void PerfAnalysis::visitOp(const OpNode* node){
    if(is_init_){ init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitOpLoop(node);
}

void PerfAnalysis::visitTrans(const TransNode* node){
    if(is_init_){ init(node); return; }
    if(is_get_offset_){ getOffset(node); return; }
    visitTransLoop();
}

void PerfAnalysis::init(const Node* node){
    initOffset(node);
    initTensor(node);
}

void PerfAnalysis::initTensor(const Node* node){
    
    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
        // finish one child, then update its child's tensor to itself
        current_node_->updateTensor(current_node_->get_children()[i]->input_tensors_,
                                   current_node_->get_children()[i]->output_tensors_);
    }

    // no child -- OP Trans
    if (current_node_->ori_node_->get_type() == Node::Op){
        auto name = NameSub(current_node_->ori_node_->get_name());
        auto tensorName = GetInOutTensor(name);
        auto dimName = GetDimName(tensorName);
        for (unsigned i = 0; i < tensorName.size(); i++){
            auto tensor = tensorName[i];
            auto tensor_dim = dimName[tensor];
            TensorMap tensorMap(tensor, tensor_dim);
            if(i == tensorName.size() - 1){
                // current_node_->output_tensors_[tensor] = tensorMap;
                current_node_->add_input_tensors(tensorMap, tensor);
            }
            else{
                // current_node_->input_tensors_[tensor] = tensorMap;
                current_node_->add_output_tensors(tensorMap, tensor);
            }
        }
    }
    //Trans -- nothing to do
    else{}

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::initOffset(const Node* node){
    // current_node_->clear_IO_tensors();
    current_node_->InitDimOffset();

    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

void PerfAnalysis::getOffset(const Node* node){

    // DFS
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
                // scope: 如果是scope的话，只需要读取scope的孩子的offset即可
                if(child->get_type() == Node::Scope){
                    for (unsigned j = 0; j < child->get_children().size(); j++){
                        auto grandson = child->get_children()[j];
                        auto eva_grandson = eva_child->get_children()[j];
                        for (auto tmp_loop: grandson->loopnests_){
                            current_node_->add_dim_offset(tmp_loop.name_, 
                                    tmp_loop.end * eva_grandson->get_dim_offset()[tmp_loop.name_]);
                        }
                    }
                }
                // tile
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
            for (auto tmp_loop: node->loopnests_){
                current_node_->add_dim_offset(tmp_loop.name_, 1);
            }
        }
    }

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}


// 计算data move的几个步骤
// 每一个op都会根据offset计算产生一个input和output tensor，以及每一个维度的循环带来的range，
// 作为data movement
// 每一个tile都会根据自身的tiling factor，对每一个child的io tensor进行维度变化
// 目前只考虑输入时op节点倍数的情况
void PerfAnalysis::visitOpLoop(const Node* node){
    
    auto dim_bound = node->loopnests_.size();

    for (unsigned i = 0; i < dim_bound; i++){ 
        current_loop_state_.push_back(current_node_->loopnests_[i]);
    }

    std::string name = NameSub(node->name_);
    std::vector<std::string> tensorName = GetInOutTensor(name);
    std::map<std::string, std::vector<std::string> > dimName = GetDimName(tensorName);

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


void PerfAnalysis::visitTileLoop(const Node* node, unsigned current_dim_idx){
    
    // 包含几个维度
    auto dim_bound = node->loopnests_.size();
    
    // 如果当前tile的几个维度已经遍历完，则进行继续的DFS访问
    if (current_dim_idx == dim_bound){
        // // 遍历全部child，获取tensor信息
        // // PrintDimLoop("K");
        // for (unsigned i = 0 ; i < node->get_children().size(); i++){
        //     current_node_ = current_node_->get_children()[i];
        //     node->get_children()[i]->accept(this);
        // }
        return;
    }

    std::string dim_name_ = current_node_->loopnests_[current_dim_idx].name_;
    int ori_start = current_node_->loopnests_[current_dim_idx].start;
    int* loop_start = &(current_node_->loopnests_[current_dim_idx].start);
    bool is_last_dim;
    int loop_stride = current_node_->loopnests_[current_dim_idx].stride;
    TILEEXP_ASSERT(loop_stride == 1, "currently only support stride = 1");
    
    if (node->parent_ == nullptr || isLastLoop(dim_name_)){ is_last_dim = true; }
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
        
        std::pair<int, int> loop_range = {*loop_start, loop_end};
        current_dim_range_[dim_name_].push_back(loop_range);
        current_offset_[dim_name_].push_back(current_node_->dim_offset_[dim_name_]);
        current_node_->node_dim_bound_[dim_name_] = loop_range;
        // 准备计算维度范围和张量范围
        if (current_dim_idx + 1 < dim_bound){
            visitTileLoop(node, current_dim_idx + 1);
        }
        else{
            // // 只会在这里进行DM和Latency的计算
            // // AddChildTensorMap();
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
        // 实际计算维度范围和张量范围
        // CalculateDataMovement();
        // CalculateLatency();
        vec_last_dim_[dim_name_].pop_back();
        current_offset_[dim_name_].pop_back();
        current_dim_range_[dim_name_].pop_back();
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
    // if(vec_last_loop.size() == 0){
    //     return false;
    // }
    for(auto last_loop: vec_last_loop){
        tmp = tmp && last_loop;
    }
    return tmp;
}


std::vector<std::string> PerfAnalysis::GetInOutTensor(std::string name){
    std::vector<std::string> res;
    auto workload = evaluator_.workloads_.get_workload(name);
    for (auto& dataSpaceName: workload->get_ins()){
        res.push_back(dataSpaceName);
    }
    res.push_back(workload->get_out());
    return res;
}

// last dimName is the output
std::map<std::string, std::vector<std::string> > PerfAnalysis::GetDimName(std::vector<std::string> tensorName){
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


// void SimAnalysis::run(const Node* root){
//     // init
//     is_init_ = true;
//     root->accept(this);
//     is_init_ = false;
//     // get offset
//     is_get_offset_ = true;
//     root->accept(this);
//     is_get_offset_ = false;

//     // compute tensor range
//     root->accept(this);
// }

// // Init and Get loop offset, store in SimAnalysis
// void SimAnalysis::Init(const Node* node){
//     // current_node_->clear_IO_tensors();
//     current_node_->InitDimOffset();

//     for (unsigned i = 0 ; i < node->get_children().size(); i++){
//         current_node_ = current_node_->get_children()[i];
//         node->get_children()[i]->accept(this);
//     }
//     current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
// }

// void SimAnalysis::getOffset(const Node* node){

//     // node->loopnests_;
//     // current_node_->

//     for (unsigned i = 0 ; i < node->get_children().size(); i++){
//         current_node_ = current_node_->get_children()[i];
//         node->get_children()[i]->accept(this);
//     }

//     std::map<std::string, int> tmp_dim_offset_;
//     if (node->get_children().size() != 0){
//         if(node->get_type() == Node::Scope){ }
//         else{
//             for (unsigned i = 0; i < node->get_children().size(); i++){
//                 auto child = node->get_children()[i];
//                 auto eva_child = current_node_->get_children()[i];
//                 if(child->get_type() == Node::Scope){
//                     for (unsigned j = 0; j < child->get_children().size(); j++){
//                         auto child2 = child->get_children()[j];
//                         auto eva_child2 = eva_child->get_children()[j];
//                         for (auto tmp_loop: child2->loopnests_){
//                             current_node_->add_dim_offset(tmp_loop.name_, 
//                                     tmp_loop.end * eva_child2->get_dim_offset()[tmp_loop.name_]);
//                         }
//                     }
//                 }
//                 else {
//                     for (auto tmp_loop: child->loopnests_){
//                         current_node_->add_dim_offset(tmp_loop.name_, 
//                                                     tmp_loop.end * eva_child->get_dim_offset()[tmp_loop.name_]);
//                     }
//                 }
//             }
//         }
//     }
//     else{
//         if(node->get_type() == Node::Trans){ }
//         else{
//             for (auto tmp: node->loopnests_){
//                 current_node_->add_dim_offset(tmp.name_, 1);
//             }
//         }
//     }

//     current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
// }

// bool SimAnalysis::isLastLoop(std::vector<bool> vec_last_loop){
//     bool tmp = true;
//     for(auto last_loop: vec_last_loop){
//         tmp = tmp && last_loop;
//     }
//     return tmp;
// }

// void SimAnalysis::visitTileLoop(const Node* node, unsigned current_loop_idx, bool is_last_loop){
//                             // std::vector<loop::TileExp::Descriptor> loop_state){
    
//     if (node->parent_ == nullptr || isLastLoop(vec_last_loop_)){ is_last_loop = true; }
//     auto loop_bound = node->loopnests_.size();

//     if (current_loop_idx == loop_bound){
//         // 遍历全部child，获取tensor信息
//         for (unsigned i = 0 ; i < node->get_children().size(); i++){
//             current_node_ = current_node_->get_children()[i];
//             node->get_children()[i]->accept(this);
//         }
//         // // 根据child的tensor信息以及父节点的scope信息，构建当前节点的tensormap
//         // auto parent_type = current_node_->type_();
//         // for (unsigned i = 0 ; i < node->get_children().size(); i++){
//         //     auto tmp_node = current_node_->get_children()[i];
//         // }
//         return;
//     }
    
//     auto dim_name_ = current_node_->loopnests_[current_loop_idx].name_;

//     int ori_start = current_node_->loopnests_[current_loop_idx].start;
//     int* loop_start = &(current_node_->loopnests_[current_loop_idx].start);
//     int loop_end;
//     if (is_last_loop){
//         loop_end = current_node_->loopnests_[current_loop_idx].end + 
//                    current_node_->loopnests_[current_loop_idx].residual_end;
//     }
//     else{
//         loop_end = current_node_->loopnests_[current_loop_idx].end;
//     }
//     int loop_stride = current_node_->loopnests_[current_loop_idx].stride;
//     assert(loop_stride == 1);

//     for (; *loop_start < loop_end; *loop_start += loop_stride){
//         current_loop_state_.push_back(current_node_->loopnests_[current_loop_idx]);
//         dim_offset_all_[dim_name_].push_back(current_node_->dim_offset_[dim_name_]);
//         vec_last_loop_.push_back(*loop_start == loop_end - 1);

//         visitTileLoop(node, current_loop_idx + 1, is_last_loop);
        
//         vec_last_loop_.pop_back();
//         dim_offset_all_[dim_name_].pop_back();
//         current_loop_state_.pop_back();
//     }
//     if (current_loop_idx == 0){
//         current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
//     }
//     // 还原初始start
//     *loop_start = ori_start;
// }


// std::vector<std::string> SimAnalysis::GetInOutTensor(std::string name){
//     std::vector<std::string> res;
//     auto workload = evaluator_.workloads_.get_workload(name);
//     for (auto& dataSpaceName: workload->get_ins()){
//         res.push_back(dataSpaceName);
//     }
//     res.push_back(workload->get_out());
//     return res;
// }

// // last dimName is the output
// std::map<std::string, std::vector<std::string> > SimAnalysis::GetDimName(std::vector<std::string> tensorName){
//     // auto workload = evaluator_.workloads_.get_workload(name);
//     auto common_shape = evaluator_.workloads_.get_shape();
//     // input
//     std::vector<std::string> tmp_dim_name;
//     std::map<std::string, std::vector<std::string> > res_dim_name;
//     for (auto& dataSpaceName: tensorName){
//         tmp_dim_name.clear();
//         auto dataSpaceID = common_shape.DataSpaceNameToID[dataSpaceName];
//         auto& proj = common_shape.Projections[dataSpaceID];
//         for (auto& expr: proj){
//             for (auto& term: expr){
//                 tmp_dim_name.push_back(common_shape.FactorizedDimensionIDToName[term.second]);
//             }
//         }
//         res_dim_name[dataSpaceName] = tmp_dim_name;
//     }
//     return res_dim_name;
// }

// // 计算data move的几个步骤
// // 每一个op都会根据offset计算产生一个input和output tensor，以及每一个维度的循环带来的range，
// // 作为data movement
// // 每一个tile都会根据自身的tiling factor，对每一个child的io tensor进行维度变化
// void SimAnalysis::visitOpLoop(const Node* node){
    
//     auto loop_bound = node->loopnests_.size();

//     for (unsigned i = 0; i < loop_bound; i++){ 
//         current_loop_state_.push_back(current_node_->loopnests_[i]);
//     }

//     // auto offset = ;
//     // for (auto loop_tmp : current_loop_state_){
//     //     std::cout << loop_tmp.name_ << loop_tmp.start << "  ";
//     // }
//     // std::cout << std::endl;

//     std::string name = NameSub(node->name_);
//     std::vector<std::string> tensorName = GetInOutTensor(name);
//     std::map<std::string, std::vector<std::string> > dimName = GetDimName(tensorName);

//     // 每一个循环为当前OP节点添加一个tensormap
//     DimRange dimRange;
//     std::vector<DimRange> dimRangeVec;
//     for(unsigned i = 0; i < tensorName.size(); i++){
//         // output
//         dimRangeVec.clear();
//         if(i == tensorName.size() - 1){
//             auto tensor = tensorName[i];
//             auto tensor_dim = dimName[tensor];
//             for(auto dim_name: tensor_dim){
//                 dimRange.dim_name_ = dim_name;
//                 dimRange.low_bound_ = 0;
//                 if(isLastLoop(vec_last_loop_)){
//                     dimRange.high_bound_ = 0;
//                 }
//                 dimRangeVec.push_back(dimRange);
//             }
//             TensorMap tensorMap(tensor, tensor_dim, dimRangeVec);
//             current_node_->output_tensors_.push_back(tensorMap);
//         }
//         // input
//         else{   
//             auto tensor = tensorName[i];
//             auto tensor_dim = dimName[tensor];
//             for(auto dim_name: tensor_dim){
//                 dimRange.dim_name_ = dim_name;
//                 dimRange.low_bound_ = 0;
//                 if(isLastLoop(vec_last_loop_)){
//                     dimRange.high_bound_ = 0;
//                 }
//                 dimRangeVec.push_back(dimRange);
//             }
//             TensorMap tensorMap(tensor, tensor_dim, dimRangeVec);
//             current_node_->input_tensors_.push_back(tensorMap);
//         }
//     }

//     for (unsigned i = 0; i < loop_bound; i++){ 
//         current_loop_state_.pop_back();
//     }
//     current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
// }

// void SimAnalysis::visitScopeLoop(const Node* node){

//     for (unsigned i = 0 ; i < node->get_children().size(); i++){
//         current_node_ = current_node_->get_children()[i];
//         node->get_children()[i]->accept(this);
//     }
//     current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
// }

// void SimAnalysis::visitTransLoop(){
//     current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
// }

// // recursive for loop
// // 第一个循环做嵌套的loopnest的循环，间隔为stride，首尾为start，end+residual
// // 如果是spatial。。。。
// // 第二个循环做child级别的，不同child之间的关系根据scope来
// // 每一个child都需要在当前循环建立一个tensormap：这里根据input和output的不同会有不一样的data movement
// // 需要特殊处理Op，scope和trans节点
// // 只计算0->1,1->2的数据*(loop-1)
// // 各个child的tensormap根据scope的关系得出一个大的tensormap为父节点的tensormap

// void SimAnalysis::visitScope(const ScopeNode* node){
//     if(is_init_){ Init(node); return; }
//     if(is_get_offset_){ getOffset(node); return; }
//     visitScopeLoop(node);
// }

// void SimAnalysis::visitTile(const TileNode* node){
//     if(is_init_){ Init(node); return; }
//     if(is_get_offset_){ getOffset(node); return; }
//     visitTileLoop(node, 0, false);
// }

// void SimAnalysis::visitOp(const OpNode* node){
//     if(is_init_){ Init(node); return; }
//     if(is_get_offset_){ getOffset(node); return; }
//     visitOpLoop(node);
// }

// void SimAnalysis::visitTrans(const TransNode* node){
//     if(is_init_){ Init(node); return; }
//     if(is_get_offset_){ getOffset(node); return; }
//     visitTransLoop();
// }

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


// int32_t GetLoopCount::DfsLoop(std::vector<int32_t> dim_end, std::vector<int32_t> dim_res, bool is_last){
//     int32_t result = 1;
//     if(is_last){
//     }
// }



// std::map<std::string, int> GetLoopCount::get_loop_count(
//     std::vector<std::vector<loop::TileExp::Descriptor>> input_loopnests){   
// }

} // namespace Analysis
} // namespace TileExp