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
#include "./TileExp/common.hpp"
#include "./TileExp/problem/problem.hpp"


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

  if (root.exists("arch")){
    arch = root.lookup("arch");
  }
  else if (root.exists("architecture")){
    arch = root.lookup("architecture");
  }

  auto problem = root.lookup("problem");
  problem::TileExp::Workloads workloads_instance;

  bool is_sparse_topology = root.exists("sparse_optimizations"); // we don't consider this

  // node information hide in model::Engine::Specs::topology
  model::Engine::Specs arch_specs_ = model::Engine::ParseSpecs(arch, is_sparse_topology); // only for sepc.topology

  if (root.exists("ERT")) // we don't consider this
  {
    std::cout << "Found Accelergy ERT (energy reference table), replacing internal energy model." << std::endl;
    auto ert = root.lookup("ERT"); 
    arch_specs_.topology.ParseAccelergyERT(ert);
    if (root.exists("ART")){ // Nellie: well, if the users have the version of Accelergy that generates ART
      auto art = root.lookup("ART"); 
      arch_specs_.topology.ParseAccelergyART(art);  
    } 
  }

  // // ***** test ***** //
  // auto topology = arch_specs_.topology;
  // auto level_spec = topology.GetLevel(2);
  // std::cout << "get success" << std::endl;
  // model::BufferLevel::Specs& current_specs = *std::static_pointer_cast<model::BufferLevel::Specs>(level_spec);
  // // std::cout << current_specs.className << current_specs.layout << std::endl;
  // for(auto& tmp: current_specs.successor.Get()){
  //   std::cout << tmp << std::endl;
  // }
  // std::cout << "cast success" << std::endl;
  // // copy cosntruct error -- new element?
  // std::shared_ptr<model::BufferLevel> buffer_level = std::make_shared<model::BufferLevel>(current_specs); 
  // std::cout << "ptr success" << std::endl;
  // // ***** end test ***** //

  // build architecture graph  
  model::TileExp::Graph graph(arch_specs_);
  if(TileExp::verbose_level) graph.Print();

  std::cout << "Begin ParseWorkload..." << std::endl;
  problem::TileExp::ParseWorkloads(problem, workloads_instance); // analysis prob yaml
  problem::Workload::SetCurrShape(&workloads_instance.get_shape()); // set problem namespace --> current_shape_

  if (TileExp::verbose_level)  
    workloads_instance.Print();

  return 0;
}