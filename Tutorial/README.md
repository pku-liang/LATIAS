# Tutorial

## 概述
我们提出了一种针对华为智能芯片上的算子性能建模及分析方法。我们首先通过使用图表示的形式，对硬件架构及互联方式进行建模；其次，引入算子循环树节点的链接机制，使得算子循环能够在图表示下的硬件架构中进行描述和映射，最后，使用流水线分析方法对多种组合形式下的单算子及融合算子进行延迟性能分析。

## 能够完成的功能

1. 描述算子功能和分块映射策略，支持单算子和有限的融合算子；
2. 描述硬件连接关系及硬件参数；
3. 通过对算子和硬件的描述，分析得出该算子在当前硬件下的性能；


## 运行方式

    ${Workspace}/build/TileExp ${TargetYamlFolder}/*.yaml

## 输入文件

输入文件分为以下四类：
### 1. 架构文件
该文件负责描述硬件架构信息

    1. 文件起始需要标注architecture关键词
    2. 通过subtree描述同级别的计算/存储单元节点
    3. 每一个计算/存储节点均需要描述其名称、并行个数（level_num，指完全独立的并行，比如910B3包含20个Cube core，那么L1的level_num就是20）、类型（如DRAM，SRAM，VEC，CUBE），以及其对应的属性（attributes），在attributes下的其他属性暂未被使用，描述是需要通过缩进描述
    4. 对于计算节点，attributes下会有一个compute cycle，该功能用于描述该计算节点完成一次完整的计算所需要的cycle数，包括对该计算节点的搬入数据、搬出数据时间以及计算时间

节点参考实例

    - name: L0AB
      level_num: 20
      level_name: SRAM 
      attributes:
          meshX: 20 
          meshY: 1
          depth: 64 
          block_size: 1024
          word-bits: 8

### 2. 互联文件
该文件负责描述硬件架构信息中的有向互联关系

    1. 文件起始需要标注interconnection关键词
    2. 通过connection描述连接关系
    3. 每一个计算/存储节点均需要描述其名称、起始终止的计算/内存层级名称（双向均需要描述）、带宽（TBD）、以及互联类型（包括全双工FD、半双工HD、单播UC、广播BC）
    4. 带宽的单位为（`2Byte`/cycle），之所以是2Byte是因为，我们默认数据为16 bit的输入形式。如L1->L0AB的带宽实测约为7.2TB/s，则我们这里用7.2TB/1800MHz(cycle数)/20(核数)/2(16 bit输入) = 100

节点参考实例

    - name: C4
        source: L1
        target: L0AB
        type: HD
        bandwidth: 100

### 3. 算子描述文件
该文件负责描述算子的功能性

    1. 

节点参考实例

    problem:
      io:
        ins: A, B
        outs: C
      dimensions: [M, N]
      instance:
        M: 512
        N: 4096

      ops:
      - name: Sub
        dimensions: [M,N]
        type: VECTOR
        data-spaces:
        - name: C
          projection:
            - [[M]]
            - [[N]]
          read-write: True
        - name: A
          projection:
            - [[M]]
            - [[N]]
        - name: B
          projection:
            - [[M]]
            - [[N]]
        ins: A, B
        out: C

### 4. 映射文件
该文件负责描述硬件架构信息中的有向互联关系


## 当前限制

1. 融合算子下仅支持连续的融合，也即支持形如C=A+B，E=C+D这种，前一个计算的结果直接被下一个计算使用的融合，暂不支持如C=A+B，F=D+E，Out=C+F，这种中间插入运算的融合；
2. 循环迭代时，当前仅支持循环迭代数每次+1
3. 假定MainMemory（如HBM）的容量上限无穷（单算子级别一般也用不完）
4. 假定cache hit rate = 1，也即数据均从cache读写，如若需要考虑从HBM读取带来的影响，则需要想办法估算算子的cache hit rate，并据此调节带宽

## 其他资源

如需修改代码以增补功能或提供更细致的硬件使用，请参考CODE.md

如对输入文件中所涉及的其他名词感兴趣，请参考timeloop官网，https://timeloop.csail.mit.edu/
