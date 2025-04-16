# Code structure:

## parser: done
1. arch (basic hardware node info)
2. interconnection (node interconnection info, contains duplex, cast, NoC) -- connection category
3. prob (point out the calculation place)
4. map (user-defined mapping, if doesnt exist, then deduce the mapping)

## build graph: done

### hardware and interconnection
use arch and interconnection yaml

start from the root (DRAM), then iteratively visit interconnection yaml, add nodes to the graph 

use dictionary to store hardware graph and interconnection -- external


### problem
follow the tileflow, add the calculation PE information

### mapping

if:
    
    does not exist mapping, then deduce from the problem space
else:
    
    1. start from the root (DRAM), map every operator info to hardware -- output a mapping graph
    2. operator node contains: Compute node, memory node (or tiling node), and scope node (seq,shar,pipe,paral,Ring,Mesh), trans node
    3. Node basic info: dataflow mode, node type, loopnest_, parent, children, target_level_name, storage_level_name, interconnection


## check graph

1. check if current interconnection is legal 
2. check if the tiling factor statisfies the original loop -- done
3. check if the tiling factor and mapping statisfy the memory constraints

## analysis
1. 获取根节点，DFS遍历
2. 获取节点中的当前loop循环，包括维度和循环数（其中需要判断是否当前循环为residual的循环--根据parent的结果，获取当前的start end）
1. 维护一个类的dim vector,用于统计上层循环的位置
1. 嵌套loop表示循环(通过stack实现)
3. 逐个计算子节点所需的张量范围（通过类中的stack维护，向下遍历时push，反之pop），每次均保留至当前节点，保留的情况根据scope决定，每次保留的过程中计算循环+stride时的数据搬运量，同时，需要维护一个已经需要的变量的list
1. 计算子节点的过程中，同时返回子节点所需的延迟（三段+类型）和数据搬运量，用一个unordered_map<string, vector>描述
4. 结合数据搬运量、子节点延迟、传输类型以及当前scope的类型计算当前节点延迟
5. 重复循环

每个循环中维护三种类型的循环变量，分别记录初次，中间，结尾


## search



## Analysis Code

    for tile
        if not end dim:
            recursive tile
        else
            for i in child.size():
                currenet_scope_ = parent_scope or pipeline;
                current_intra_ = spa or tmp
                if child[i] == OP:
                    Tensor = getIOTensor(node); 
                    AddTensorToCurrentNode(); // 允许重合
                    New TensorRange_tmp = AddTensorMapWithRange(); // child_tensor_map
                    IODM(TensorRange_tmp, TensorRange[i])
                    Latency(); // 3 stage
                if child[i] == Scope:
                    keep going
                    //在这里需要聚集其下的tile的latency，供上级tile的spatial or temporal处理
                if child[i] == Tile:
                    run(child[i])
                    getIOTensor(node); 
                    AddTensorMapWithRange();
                    IODM();
                    Latency(); // 3 stage -- 5 stage, add sum, max
                if child[i] == Transmit:
                    getOTensor(); // for child[i-1]
                    // ODM();
                    Latency();


## init analysis

1. 找到每一个OP的IO，分别加进analysis的producer和consumer中，其中OP也需要存放，包含OP name，address和具体的tensor
2. 逐个查找每一个OP的Input的tensor，观测其是否存在于producer中
    1. 如果存在，则逐个遍历OP list，查找到生产者的位置，将生产者-公共节点（除公共节点）的output_tensor添加上对应tensor，将公共节点-消费者（除公共节点）的input_tensor添加上对应tensor
    2. 若不存在，则遍历全部父节点，为其input_tensor添加对应tensor
3. 若访问到最后一个OP，则为其上的全部父节点的output_tensor添加对应output tensor


author list

    Chengrui Zhang <zhangchr@stu.pku.edu.cn>
    Chu Wang <2100013096@stu.pku.edu.cn>
    Tianqi Li <jsnjltq@stu.pku.edu.cn>
    Renze Chen <crz@pku.edu.cn>
    Size Zheng <zheng.size@bytedance.com>
    Liancheng Jia <jlc@pku.edu.cn>
    Xiuhong Li <lixiuhong@pku.edu.cn>
    Shengen Yan <yansg@tsinghua.edu.cn>
    Youwei Zhuo <youwei@pku.edu.cn>
    Yu Wang <yu-wang@tsinghua.edu.cn>
    Yun Liang <ericlyun@pku.edu.cn>

title 

    LATIAS: A General Performance Model for Spatial Architecture with complex topology and memory hierarchy.

abs

    We propose a general dataflow analysis model, LATIAS, to facilitate analysis of various workloads on complex spatial accelerator architectures. The key innovation of our framework is a unified graph-based representation to represent computational units, memory hierarchies, and interconnection, enabling general modeling of spatial accelerators with complex data paths and dataflow of operators mapped onto it. LATIAS employs the graph-based description and a general tile-centric notation to analyze data movement, latency, memory constraint and fusion possibility on this graph to assess workload performance on the target architecture and conducts SA-based design space exploration (DSE) to optimize tiling factors and fusion strategies. Finally, the DSE results are deployed on the Huawei's Ascend 910B series spatial accelerators to assess performance. Our experimental results show that the fused operators identified by our model achieve an XXX speedup over the official operator library, with a performance correlation of XXX with the official operator performance. 