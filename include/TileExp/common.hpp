#pragma once 

#define MCARO_WRAPPER(code) do {code} while(0) 

#define TILEEXP_ERROR(msg) do{std::cerr << "[ERROR]: " << msg << std::endl;exit(1);} while(0)

#define TILEEXP_ASSERT(cond, msg) do{if(!(cond)) {std::cerr << "[ASSERT ERROR]: " << msg << std::endl; exit(1);} }while(0)

#define TILEEXP_WARNING(msg) do{std::cerr << "[WARNING]: " << msg << std::endl;}while(0)

#define TILEEXP_LOG(msg) do{std::cerr << "[LOG]: " << msg << std::endl;}while(0)

#define TILEEXP_COND_WARNING(cond, msg) do{if(!(cond)) {std::cerr << "[WARNING]: " << msg << std::endl;}}while(0)

#define TILEPRINT(msg) do{std::cout << msg << std::endl; exit(1);} while(0)

#include "compound-config/compound-config.hpp"

const int MaxTensors = 32;

namespace TileExp {

extern int verbose_level;

extern config::CompoundConfigNode macros;

}