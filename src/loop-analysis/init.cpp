#include "TileExp/loop-analysis/analysis.hpp"   
#include "TileExp/mapping/mapping.hpp"

// ************************ Init Analysis part *************************

namespace TileExp{

namespace Analysis{
    
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

// TBD -- 需要改变，需要根据实际的连接关系，为不同的内存层级添加张量信息 -- done
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

    auto source_idx = std::find(leaf_vec_.begin(), leaf_vec_.end(), source);
    auto target_idx = std::find(leaf_vec_.begin(), leaf_vec_.end(), target);

    auto distance = std::distance(source_idx, target_idx);

    if (distance == 1){
        source_path.erase(source_path.begin() + i - 1, source_path.end()); // 包含公共祖先，此时为写回模式
    }
    else{
        source_path.clear();
        for(int i = 1; i < distance; i++){
            source_path.push_back(*(source_idx + i)); // 囊括trans node，暂不支持非连续op节点融合
        }
    }
    target_path.erase(target_path.begin() + j, target_path.end()); // 不包含公共祖先

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
        leaf_vec_.push_back(current_node_); // 在这里完成对叶子节点的统计

        for (unsigned i = 0; i < tensorName.size(); i++){
            auto tensor = tensorName[i];
            auto tensor_dim = dimName[tensor];
            TensorMap tensorMap(tensor, tensor_dim);
            if(i == tensorName.size() - 1){
                current_node_->add_output_tensors(tensorMap, tensor); // 目前的是仅在当前的Node下做output tensor的插入
                std::pair<TensorMap, EvaNode*> tensorMap_pair(tensorMap, current_node_);
                TILEEXP_ASSERT(producer_map_.insert({tensor, tensorMap_pair}).second, "Duplicate producer tensor");
            }
            else{
                current_node_->add_input_tensors(tensorMap, tensor);
            }
        }
    }

    if (current_node_->ori_node_->get_type() == Node::Trans){
        leaf_vec_.push_back(current_node_); // 在这里完成对叶子节点的统计
    }

    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
}

// 将当前节点的全部维度的offset初始化为1
void InitAnalysis::initOffset(const Node* node){
    // current_node_->clear_IO_tensors();
    current_node_->initDimOffset();

    for (unsigned i = 0 ; i < node->get_children().size(); i++){
        current_node_ = current_node_->get_children()[i];
        node->get_children()[i]->accept(this);
    }
    current_node_ = current_node_->get_parent() != nullptr? current_node_->get_parent() : current_node_;
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


}
 // end namespace
}