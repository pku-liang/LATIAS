# 程序入口
./src/application/main.cpp

# 程序架构

    1. 首先通过基于timeloop的parser将数据从yaml文件中读取出，并基于此构建描述硬件架构的arch和arch图、描述互联的interCon、描述工作负载的problem以及描述映射关系的mapping
    2. 获取基本对象后，构建评估器evaluator对上述对象进行分析
        a. 分析过程在./include/TileExp/loop-analysis和对应的src文件夹中
        b. 分析步骤主要分为四个步骤，在./TileExp/src/loop-analysis/common.cpp中，包括重置分析、合法性验证、初始化分析、以及实际分析



# 基本数据结构

基本数据结构在./include/TileExp/下model、problem和mapping文件夹中，用于构建软硬件结构