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
#include "./TileExp/mapping/mapping.hpp"
#include "./TileExp/mapping/parser.hpp"


void show_energy(
  const model::Engine::Specs& EngineSpecs,
  std::ostream& o = std::cout) {

  auto specs_ = EngineSpecs.topology;
  o << "==========AccessEnergy===========" << std::endl;
  // o << "metric, energy" << std::endl;
  for (unsigned i = 0; i < specs_.NumLevels(); i++) {
    auto level = specs_.GetLevel(i);
    if (level->Type() == "ArithmeticUnits"){
      auto level_specs = std::static_pointer_cast<model::ArithmeticUnits::Specs>(level);
      o << "Arith::" << level->level_name << "::energy_per_op," 
        << level_specs->op_energy_map.at("random_compute") << std::endl;
    }
    else if (level->Type() == "BufferLevel"){
      auto level_specs = std::static_pointer_cast<model::BufferLevel::Specs>(level);
      o << "Buffer::" << level_specs->level_name << "::energy_per_op::read," << level_specs->op_energy_map.at("random_read") << std::endl;
      o << "Buffer::" << level_specs->level_name << "::energy_per_op::update," << level_specs->op_energy_map.at("random_update") << std::endl;
      o << "Buffer::" << level_specs->level_name << "::energy_per_op::fill," << level_specs->op_energy_map.at("random_fill") << std::endl;
    }
  }
  o << "========End AccessEnergy=========" << std::endl;
}

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
  mapping::LevelName2IdxMap = arch_specs_.topology.GetName2IdxMap();

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

  // print buffer words
  for (unsigned storage_level_id = 0; storage_level_id < arch_specs_.topology.NumLevels();
      storage_level_id ++){
    if (storage_level_id < arch_specs_.topology.ArithmeticMap() + 1) continue;
    auto bufferlevel = arch_specs_.topology.GetLevel(storage_level_id);
    auto bufferspecs = std::static_pointer_cast<model::BufferLevel::Specs>(bufferlevel);
    TILEEXP_COND_WARNING(bufferspecs->size.IsSpecified(), "No memory size specified at " << bufferspecs->name.Get());
    if (TileExp::verbose_level) {
      std::cout << bufferspecs->name.Get() << ": ";
      std::cout << bufferspecs->size.Get() << " words" << std::endl;
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

  // parse workload -- tileflow mode
  std::cout << "Begin ParseWorkload..." << std::endl;
  problem::TileExp::ParseWorkloads(problem, workloads_instance); // analysis prob yaml
  problem::Workload::SetCurrShape(&workloads_instance.get_shape()); // set problem namespace --> current_shape_

  if (TileExp::verbose_level)  
    workloads_instance.Print();

  if (TileExp::verbose_level)
    show_energy(arch_specs_, std::cout);
  
  // note that we do not consider the network specs 

  // parse mapping -- input config, graph topology and workloads
  // build symbol table here
  auto mapping = 
    mapping::TileExp::ParseAndConstruct(root.lookup("mapping"), graph, workloads_instance, arch_specs_); // parse mapping
  

  return 0;
}