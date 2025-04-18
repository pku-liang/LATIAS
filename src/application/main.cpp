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
#include "./TileExp/model/interconnection.hpp"
#include "./TileExp/common.hpp"
#include "./TileExp/problem/problem.hpp"
#include "./TileExp/mapping/mapping.hpp"
#include "./TileExp/mapping/parser.hpp"
#include "./TileExp/mapper/checker.hpp"
#include "./TileExp/loop-analysis/analysis.hpp"


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


  Hardware::ArchTopology::ArchTopo archTopo_;
  Hardware::ArchTopology::ParseArchTopology(arch, archTopo_);

  auto interconnection = root.lookup("interconnection");
  Hardware::InterConnection::InterCon interCon_;
  Hardware::InterConnection::ParseInterConnection(interconnection, interCon_, archTopo_); // parse interconnection

  if (TileExp::verbose_level){
    archTopo_.Print();
    interCon_.Print();
  }

  mapping::LevelName2IdxMap = archTopo_.GetArchTopoName2IdxMap();
  // mapping::LevelName2TypeMap = archTopo_.topology.GetLevelName2TypeMap();

  auto problem = root.lookup("problem");
  problem::TileExp::Workloads workloads_instance_;
  
  // parse workload -- tileflow mode
  std::cout << "Begin ParseWorkload..." << std::endl;
  problem::TileExp::ParseWorkloads(problem, workloads_instance_); // analysis prob yaml
  problem::Workload::SetCurrShape(&workloads_instance_.get_shape()); // set problem namespace --> current_shape_

  if (TileExp::verbose_level)  
    workloads_instance_.Print();
  
  // parse mapping -- input workloads, interconnections and arch
  auto mapping_ = mapping::TileExp::ParseMapping(root.lookup("mapping"), workloads_instance_, archTopo_, interCon_);

  if (TileExp::verbose_level)
    mapping_.Print();

  bool enable_mem_check_ = true;
  bool enable_spatial_check_ = true;
  bool enable_operation_check_ = true;
  // here we do not consider the loop count check since there might be some variables in our mapping
  bool enable_loopcount_check_ = false; 

  // check
  Check::TileExp::Checker checker_(workloads_instance_, mapping_, archTopo_, interCon_,  
    enable_mem_check_, enable_spatial_check_, enable_loopcount_check_, enable_operation_check_);

  checker_.check();

  // // evaluate data movement, latency and energy consumption
  TileExp::Analysis::Evaluator evaluator_(workloads_instance_, mapping_, archTopo_, interCon_);

  evaluator_.evaluate();

  if (TileExp::verbose_level) evaluator_.Print();

  return 0;
} 