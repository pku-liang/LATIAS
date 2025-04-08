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
