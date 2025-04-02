# Code structure:

## parser:
1. arch (basic hardware node info)
2. interconnection (node interconnection info, contains duplex, cast, NoC) -- connection category
3. prob (point out the calculation place)
4. map (user-defined mapping, if doesnt exist, then deduce the mapping)

## build graph 

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
2. check if the tiling factor statisfies the original loop 
3. check if the tiling factor and mapping statisfy the memoru constraints

## analysis
1. 

## search
