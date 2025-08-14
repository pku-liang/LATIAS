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

## latency analysis
1. 对于已经有的，当前tile(or Op)的输入输出数据搬运量，计算搬运延迟
    1. 搬入延迟的计算：1）跳过scope节点和trans节点，return 0； 2）如果该节点的上一个平级节点的最深处节点的末尾是trans节点，则该节点的数据来源于该trans节点的上一个平级节点；3）如果不存在上一个平级节点，或者不是上述情况，则该节点数据来源于上层的最近tile节点，读取出对应的带宽计算cycle数目
    2. 搬出延迟的计算：1）跳过scope节点和trans节点，return 0；2）如果该tile节点下一个平级节点是trans节点，则以trans节点的目标作为传递对象来读取带宽数据；3）如果该Op节点是Foward mode，则以找到下一个最近的trans节点的目标作为带宽计算；4）不满足以上情况，则传回至上层的最近tile节点
    3. 对于Op节点，设置compute cycle为对应Op节点的值
2. 计算当前tile的子tile的计算延迟
    1. 循环最开始时，清空当前节点的Latency vector
    2. 对于其包含的数个子tile：1）忽略trans节点；2）对于tile和op节点，读取该子节点的input output和process加入到当前节点的Latency vector；3）对于scope节点，读取孙子节点
    3. 
3. 组合构成当前tile的整体延迟，用一个结构体表示，组合时根据intra-tile的细节进行，