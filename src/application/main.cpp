#include <iostream>
#include <csignal>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>

#include "application/model.hpp"
#include "compound-config/compound-config.hpp"
#include "util/args.hpp"

#include "./TileExp/model/graph.hpp"

#include "common.hpp"

int main(int argc, char* argv[])
{
  assert(argc >= 2);
  
  std::vector<std::string> input_files;
  std::string output_dir = ".";
  bool success = ParseArgs(argc, argv, input_files, output_dir);
  if (!success)
  {
    std::cerr << "ERROR: error parsing command line." << std::endl;
    exit(1);
  }

  auto config = new config::CompoundConfig(input_files);

  auto root = config->getRoot();

  if (root.exists("macro")) 
    TileExp::macros = root.lookup("macro");
  
  if (root.exists("verbose"))
    root.lookupValue("verbose", TileExp::verbose_level);
  

  config::CompoundConfigNode arch;

  if (root.exists("arch"))
  {
    arch = root.lookup("arch");
  }
  else if (root.exists("architecture"))
  {
    arch = root.lookup("architecture");
  }
  
  bool is_sparse_topology = root.exists("sparse_optimizations"); // we don't consider this

  // node information hide in model::Engine::Specs::topology
  model::Engine::Specs arch_specs_ = model::Engine::ParseSpecs(arch, is_sparse_topology); // only for sepc.topology

  if (root.exists("ERT"))
  {
    std::cout << "Found Accelergy ERT (energy reference table), replacing internal energy model." << std::endl;
    auto ert = root.lookup("ERT"); 
    arch_specs_.topology.ParseAccelergyERT(ert);
    if (root.exists("ART")){ // Nellie: well, if the users have the version of Accelergy that generates ART
      auto art = root.lookup("ART"); 
      arch_specs_.topology.ParseAccelergyART(art);  
    } 
  }

  auto topology = arch_specs_.topology;
  auto level_spec = topology.GetLevel(2); // 有没有可能是这里的没加进去?
  std::cout << "get success" << std::endl;

  model::BufferLevel::Specs& current_specs = *std::static_pointer_cast<model::BufferLevel::Specs>(level_spec);
  // std::cout << current_specs.className << current_specs.layout << std::endl;

  // for(auto& tmp: current_specs.successor.Get()){
  //   std::cout << tmp << std::endl;
  // }

  std::cout << "cast success" << std::endl;
  // copy cosntruct error -- new element?
  std::shared_ptr<model::BufferLevel> buffer_level = std::make_shared<model::BufferLevel>(current_specs); 
  std::cout << "ptr success" << std::endl;

  model::TileExp::Graph graph(arch_specs_);
  
  std::string level_name = "MainMemory";
  unsigned arith_count = arch_specs_.topology.ArithmeticMap();
  std::cout << arith_count << std::endl;
  
  auto mem_level = arch_specs_.topology.Name2IdxMap(level_name);
  std::cout << mem_level << std::endl;

  if(mem_level < arith_count){
    auto level_specs_ = std::static_pointer_cast<model::ArithmeticUnits::Specs>(arch_specs_.topology.GetLevel(mem_level));
    std::cout << level_specs_->className << std::endl;
  }
  else{
    auto level_specs_ = std::static_pointer_cast<model::BufferLevel::Specs>(arch_specs_.topology.GetLevel(mem_level));
    std::cout << level_specs_->className << std::endl;
  }

  
  auto problem = root.lookup("problem");
//   problem::TileFlow::Workloads workloads; // 描述IO，一些具体的config，以及具体的workload bound
  
  return 0;

}