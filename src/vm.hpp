
#ifndef MUSTACHE_VM_HPP
#define MUSTACHE_VM_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <memory>
#include <string>

#include "node.hpp"
#include "data.hpp"
#include "compiler.hpp"
#include "utils.hpp"


namespace mustache {
  
class VM {
private:
  uint32_t stack[127];
  Data * dataStack[127];
  std::string lookupstr;
  Data * search(uint32_t dataStackSize, std::string * key);
  Data * searchnr(uint32_t dataStackSize, std::string * key);
public:
  std::string * execute(uint8_t * codes, int length, Data * data);
  void execute(uint8_t * codes, int length, Data * data, std::string * output);
};

}


#endif	/* MUSTACHE_VM_HPP */

