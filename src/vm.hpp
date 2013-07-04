
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


namespace mustache {
  
class VM {
public:
  std::string * execute(uint8_t * codes, int length, Data * data);
};

}


#endif	/* MUSTACHE_VM_HPP */

