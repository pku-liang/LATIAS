# Tutorial

## 概述
我们提出了一种针对华为智能芯片上的算子性能建模及分析方法。我们首先通过使用图表示的形式，对硬件架构及互联方式进行建模；其次，引入算子循环树节点的链接机制，使得算子循环能够在图表示下的硬件架构中进行描述和映射，最后，使用流水线分析方法对多种组合形式下的单算子及融合算子进行延迟性能分析。

## 能够完成的功能

1. 描述算子功能和分块映射策略，支持单算子和有限的融合算子；
2. 描述硬件连接关系及硬件参数；
3. 通过对算子和硬件的描述，分析得出该算子在当前硬件下的性能；


## 运行方式

按照上级目录中安装方式进行编译，然后：

    ${Workspace}/build/TileExp ${TargetYamlFolder}/*.yaml

## 输入文件

输入文件分为以下四类：
### 1. 架构文件
该文件负责描述硬件架构信息

    1. 文件起始需要标注architecture关键词
    2. 通过subtree描述同级别的计算/存储单元节点
    3. 每一个计算/存储节点均需要描述其名称、并行个数（level_num，指完全独立的并行，比如910B3包含20个Cube core，那么L1的level_num就是20）、类型（如DRAM，SRAM，VEC，CUBE），以及其对应的属性（attributes），在attributes下的其他属性暂未被使用，描述是需要通过缩进描述
    4. 对于计算节点，attributes下会有一个compute cycle，该功能用于描述该计算节点完成一次完整的计算所需要的cycle数，包括对该计算节点的搬入数据、搬出数据时间以及计算时间（如Cube单元可一个cycle完成16*16*16的half运算，并输出float到L0C，这整个过程消耗一个cycle）

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

    1. 文件起始需要标注problem关键词
    2. 通过io构建输入输出张量名称，其中ins表示输入张量，outs表示输出张量
    3. 通过dimension构建维度名称，用[Dim1, Dim2]的形式
    4. 通过instance构建实际维度大小，通过此即可表示算子的shape信息
    5. 通过ops构建算子信息，其下的多个算子需要保持缩进一致，对于每一个算子，都需要提供数个信息，如该计算所涉及维度（dimension）、计算类型（type，目前包括VECTOR和MATRIX）、输入输出张量（ins和out）、以及表示完整计算张量信息的data-spaces
    6. 在每一个data-spaces下的多个并行层级中，每一个层级均表示一个张量的名称和维度信息（通过projection表示）

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

  1. 文件起始需要标注mapping关键词
  2. mapping下的第一个代码块表示的是程序分析的根节点root，不同节点之间的构建关系通过subtree实现，同一个subtree下的节点为同级节点
  3. 在每一个节点中，需要描述节点具体信息，包括：1）节点类型（node-type）：节点类型包括Tile节点（描述内存层级）、OP节点（描述计算层级）、scope节点（描述节点间关系）以及Trans节点（描述传输关系）；2）数据流类型（dataflow-mode）：包括写回传输（Write-back，如UB和HBM之间）和单向传输（Forward，如L1到L0AB）；3）节点内关系类型（type）：包括空间关系（spatial）和时间关系（temporal）；4）分块参数（factors）：表示在该层级下的分块参数；5）循环顺序（permutation）：表示循环顺序，如MN表示内层循环是M，外层循环是N（TBD）；6）目标层级（target）：表示当前分块的目标层级，其中trans节点和scope节点不需要指定目标层级
  4. 对于scope节点，其可支持的描述属性为三种（顺序执行-Sequential、并行执行-Parallel、流水线执行-Pipeline），默认执行方式为pipeline，详见本目录PPT下15页内容
  5. 对于节点内关系类型，详见本目录PPT下15页内容，temporal表示该节点下的多个循环顺序执行，spatial表示该节点下的多个循环会被映射到多个硬件并行执行

节点参考实例

1）针对VECTOR单元（大小为M=16\*16\*2=512，N=64\*1\*64=4096）


    mapping:
      # 第一个分块在MainMemory（在architecture内定义的内存层级名称）上，数据流方式为写回模式，节点类型是tile节点，在该层级的分块是M维度16，N维度64，也即for N, M in zip(range(64), range(16))，节点内类型是spatial，也就意味着这16\*64=1024个循环会被分发到多个并行单元做运算
      node-type: Tile
      dataflow-mode: Write-back
      type: spatial
      factors: M=16, N=64
      permutation: MN
      target: MainMemory

      subtree:
      - node-type: Tile
        dataflow-mode: Write-back
        type: temporal
        factors: M=16, N=1
        permutation: NM
        target: UB

        subtree:
        - node-type: OP
          name: Sub
          dataflow-mode: Write-back
          factors: M=2, N=64 
          permutation: NM
          target: Vector

2）针对CUBE单元（大小为M=4\*1\*8\*16=512，N=6\*1\*8\*16=768，K=3\*2\*8\*16=768）

    mapping:
      node-type: Tile 
      dataflow-mode: Forward
      type: spatial 
      factors: M=4, N=6, K=3
      fixed: 0
      permutation: NKM
      target: MainMemory 

      subtree:
      - node-type: Tile
        dataflow-mode: Forward
        type: temporal
        factors: M=1, N=1, K=2 # 
        permutation: NKM
        target: L1 

        subtree:
        - node-type: Tile
          dataflow-mode: Forward
          type: temporal
          factors: M=8, N=8, K=8
          permutation: NKM 
          target: L0AB

          subtree: 
          - node-type: Op
            name: GEMM1
            dataflow-mode: Forward
            factors: M=16, N=16, K=16
            permutation: NKM 
            target: Cube

        - node-type: Trans
          dataflow-mode: Forward
          target: L0C

      - node-type: Trans
        dataflow-mode: Forward
        target: MainMemory


## 当前限制

1. 融合算子下仅支持连续的融合，也即支持形如C=A+B，E=C+D这种，前一个计算的结果直接被下一个计算使用的融合，暂不支持如C=A+B，F=D+E，Out=C+F，这种中间插入运算的融合；
2. 循环迭代时，当前仅支持循环迭代数每次+1
3. 假定MainMemory（如HBM）的容量上限无穷（单算子级别一般也用不完）
4. 假定cache hit rate = 1，也即数据均从cache读写，如若需要考虑从HBM读取带来的影响，则需要想办法估算算子的cache hit rate，并据此调节带宽
5. 对于异构核协同部分仍不完善，若存在融合算子同时使用了核A和核B，且两者的并行数目有区别的话，当前分析可能不准确

## 其他资源

如需修改代码以增补功能或提供更细致的硬件使用，请参考CODE.md

如对输入文件中所涉及的其他名词感兴趣，请参考timeloop官网，https://timeloop.csail.mit.edu/
