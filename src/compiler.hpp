
#ifndef MUSTACHE_COMPILER_HPP
#define MUSTACHE_COMPILER_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <memory>
#include <string>

#include "node.hpp"

namespace mustache {


typedef enum opcodes {
  /**
   * No operation
   */
  NOOP = 0x00,
  
  /**
   * Symbol marker
   */
  SYMBOL = 0x01,
  
  /**
   * Function symbol marker
   */
  FUNCTION = 0x02,
  
  /**
   * String symbol marker
   */
  STRING = 0x03,
  
  /**
   * Prints char * in symbol of the operand. Translated into PRINTL, where the
   * operand is the code location
   */
  PRINTSYM = 0x10,
  
  /**
   * Prints char * in codes point of the operand.
   */
  PRINTL = 0x11,
  
  /**
   * Prints the data specified by the pointer on the top of the execution stack
   * and pops it from the execution stack
   */
  PRINTD = 0x12,
  
  /**
   * Prints and escapes the data specified by the pointer on the top of the 
   * execution stack and pops it from the execution stack
   */
  PRINTDE = 0x13,
  
  /**
   * Pushes the location of the data from the data stack specified by the 
   * symbol of the operand onto the execution stack.
   */
  LOOKUPSYM = 0x20,
  
  /**
   * Translated version of LOOKUPSYM
   */
  LOOKUP = 0x21,
  
  /**
   * Call the function symbol specified by the operand. The current point in 
   * execution is pushed onto the execution stack
   */
  CALLSYM = 0x22,
  
  /**
   * Call the function specified by the operand. The current point in execution
   * is pushed onto the execution stack
   */
  CALL = 0x23,
  
  /**
   * Return from a function. The current execution point is popped from the top
   * of the stack
   */
  RETURN = 0x24,
  
  /**
   * If the data pointed to by the top of the execution stack is empty,
   * execute the next operation, otherwise skip
   */
  IF_EMPTY = 0x30,
          
  /**
   * If the data pointed to by the top of the execution stack is not empty,
   * execute the next operation, otherwise skip
   */
  IF_NOTEMPTY = 0x31,
  
  /**
   * If the data pointed to by the top of the execution stack is a hash,
   * jump to the operand
   */
  IF_HASH = 0x32,
} opcodes;

class CompilerState {
public:
  enum States {
    NOOP = 0x00, //opcodes::NOOP,
    SYMBOL = 0x01,  //opcodes::SYMBOL,
    FUNCTION = 0x02, //opcodes::FUNCTION,
    STRING = 0x03, //opcodes::STRING,
    END = 0x04
  };
  enum States state;
  enum States nextState;
  unsigned long pos;
  unsigned long spos;
  std::vector<uint8_t> * codes;
  CompilerState() : 
        state(States::NOOP),
        pos(0),
        codes(NULL) {
    ;
  };
  CompilerState(std::vector<uint8_t> * codes) : 
        state(States::NOOP),
        pos(0) {
    this->state = state;
    this->pos = pos;
    this->codes = codes;
  };
  States next(uint8_t * code, uint8_t * operand);
};

class CompilerSymbol {
public:
  uint16_t name;
  uint8_t type;
  std::vector<uint8_t> code;
  CompilerSymbol() : name(0), type(0) {
    
  };
};

class Compiler {
private:
  std::vector<CompilerSymbol *> symbols;
  
  CompilerSymbol * _compile(Node * node);
  void _compileIn(Node * node, CompilerSymbol * symbol);
  std::vector<uint8_t> * _serialize(CompilerSymbol * main);
  
  CompilerSymbol * getSymbol();
public:
  std::vector<uint8_t> * compile(Node * node);
  
  const char * opcodeName(uint8_t code);
  
  std::string * print(uint8_t * codes, int length);
  std::string * print(std::vector<uint8_t> * codes);
  
  static void bufferToVector(uint8_t * buffer, int length, std::vector<uint8_t> ** vector);
  static void stringToBuffer(std::string * string, uint8_t ** buffer, int * length);
  static void stringToVector(std::string * string, std::vector<uint8_t> ** vector);
  static void vectorToBuffer(std::vector<uint8_t> * vector, uint8_t ** buffer, int * length);
  
  static bool hasOperand(uint8_t code) {
    switch( code ) {
      case opcodes::CALL:
      case opcodes::CALLSYM:
      case opcodes::FUNCTION:
      case opcodes::LOOKUP:
      case opcodes::LOOKUPSYM:
      case opcodes::PRINTL:
      case opcodes::PRINTSYM:
      case opcodes::STRING:
        return true;
        break;
      default:
        return false;
    }
  };
};

}


#endif	/* MUSTACHE_COMPILER_HPP */
