
#ifndef MUSTACHE_VM_HPP
#define MUSTACHE_VM_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "node.hpp"
#include "data.hpp"
#include "compiler.hpp"
#include "utils.hpp"


namespace mustache {


/**
 * The virtual machine
 */
class VM {
private:
  //! Execution stack
  uint32_t stack[127];
  
  //! Data stack
  Data * dataStack[127];
  
  //! String for doing map lookups
  std::string lookupstr;
  
  //! Output buffer
  std::string outputBuffer;
  
  //! Search the data stack
  Data * search(uint32_t dataStackSize, std::string * key);
  
  //! Search the data stack without traversing up it
  Data * searchnr(uint32_t dataStackSize, std::string * key);
  
public:
  VM() {
    outputBuffer.reserve(1024);
  };
  
  //! Execute the VM
  std::string * execute(uint8_t * codes, size_t length, Data * data);
  
  //! Execute the VM
  void execute(uint8_t * codes, size_t length, Data * data, std::string * output);
  
  //! Execute the VM
  void execute(std::vector<uint8_t> * codes, Data * data, std::string * output);
};

}


#endif	/* MUSTACHE_VM_HPP */

