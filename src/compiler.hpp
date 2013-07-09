
#ifndef MUSTACHE_COMPILER_HPP
#define MUSTACHE_COMPILER_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstring>
#include <memory>
#include <string>

#include "node.hpp"

#define _PACKI(i, n) ((i & (0xff << n)) >> n)
#define _PACK1FN(fn, i) fn(_PACKI(i, 0))
#define _PACK2FN(fn, i) fn(_PACKI(i, 8)); _PACK1FN(fn, i)
#define _PACK3FN(fn, i) fn(_PACKI(i, 16)); _PACK2FN(fn, i)
#define _PACK4FN(fn, i) fn(_PACKI(i, 24)); _PACK3FN(fn, i)
#define _PACK1A(a, k, i) a[k] = _PACKI(i, 0)
#define _PACK2A(a, k, i) a[k] = _PACKI(i, 8); _PACK1A(a, k + 1, i);
#define _PACK3A(a, k, i) a[k] = _PACKI(i, 16); _PACK2A(a, k + 1, i);
#define _PACK4A(a, k, i) a[k] = _PACKI(i, 24); _PACK3A(a, k + 1, i);
#define _UNPACKI(i, n) (i << n)
#define _UNPACK1A(a, k) _UNPACKI(a[k], 0)
#define _UNPACK2A(a, k) _UNPACKI(a[k], 8) + _UNPACK1A(a, k + 1)
#define _UNPACK3A(a, k) _UNPACKI(a[k], 16) + _UNPACK2A(a, k + 1)
#define _UNPACK4A(a, k) _UNPACKI(a[k], 24) + _UNPACK3A(a, k + 1)

namespace mustache {


typedef enum opcodes {
  
  /** Symbols --------------------------------------------------------------- */
  
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
  
  
  
  /** Basic operations ------------------------------------------------------ */
  
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
   * Call the function symbol specified by the operand. The current point in 
   * execution is pushed onto the execution stack
   */
  CALLSYM = 0x13,
  
  /**
   * Call the function specified by the operand. The current point in execution
   * is pushed onto the execution stack
   */
  CALL = 0x14,
  
  /**
   * Return from a function. The current execution point is popped from the top
   * of the stack
   */
  RETURN = 0x16,
  
  /**
   * Jump to the operand
   */
  JUMP = 0x17,
  
  /**
   * Jump to the local position by the operand. Translated into jump
   */
  JUMPL = 0x18,
  
  /**
   * Push the operand onto the execution stack
   */
  PUSH = 0x19,
  
  /**
   * Pop the top of the execution stack
   */
  POP = 0x1a,
  
  /**
   * Increment the top of the execution stack
   */
  INCR = 0x1b,
  
  /**
   * If the top of the execution stack is greater than top - 1, then execute
   * the next operation
   */
  IF_GE = 0x1c,
  
  
  /** Data operations ------------------------------------------------------- */
  
  /**
   * Push the top of the execution stack onto the data stack
   */
  DPUSH = 0x20,
  
  /**
   * Pop the top of the data stack
   */
  DPOP = 0x21,
  
  /**
   * Pushes the location of the hash value from the hash on the top of the 
   * data stack specified by the symbol of the operand onto the data stack.
   */
  DLOOKUPSYM = 0x22,
  
  /**
   * Translated version of DLOOKUPSYM
   */
  DLOOKUP = 0x23,
  
  /**
   * Pushes the location of the hash value from the hash on the top of the 
   * data stack specified by the symbol of the operand onto the data stack.
   * Does not recurse up the data stack
   */
  DLOOKUPNRSYM = 0x24,
  
  /**
   * Translated version of DLOOKUPNRSYM
   */
  DLOOKUPNR = 0x25,
  
  /**
   * Look up the array key in the data stack specified by the top of the 
   * execution stack and push it on the top of the data stack
   */
  DLOOKUPA = 0x26,
  
  /**
   * Push the size of the array on the top of the data stack onto the 
   * execution stack
   */
  DARRSIZE = 0x27,
  
  /**
   * Prints the string data on the top of the data stack
   */
  DPRINT = 0x28,
  
  /**
   * Prints and escapes the string data on the top of the data stack
   */
  DPRINTE = 0x29,
  
  
  
  /** Data conditionals ----------------------------------------------------- */
  
  /**
   * If the data pointed to by the top of the data stack is empty,
   * execute the next operation, otherwise skip
   */
  DIF_EMPTY = 0x30,
          
  /**
   * If the data pointed to by the top of the data stack is not empty,
   * execute the next operation, otherwise skip
   */
  DIF_NOTEMPTY = 0x31,
  
  /**
   * If the data pointed to by the top of the data stack is a hash,
   * execute the next operation
   */
  DIF_HASH = 0x32,
  
  /**
   * If the data pointed to by the top of the data stack is a hash,
   * execute the next operation
   */
  DIF_NOTHASH = 0x33,
  
  /**
   * If the data pointed to by the top of the data stack is an array,
   * execute the next operation
   */
  DIF_ARRAY = 0x34,
  
  /**
   * If the data pointed to by the top of the data stack is not an array,
   * execute the next operation
   */
  DIF_NOTARRAY = 0x35,
  
  
  
  
  /**
   * If the data pointed to by the top of the data stack is not an array,
   * execute the next operation
   */
  CALLEXT = 0x36,
  
} opcodes;

class CompilerState {
public:
  enum States {
    NOOP = 0x00, //opcodes::NOOP,
    SYMBOL = 0x01,  //opcodes::SYMBOL,
    FUNCTION = 0x02, //opcodes::FUNCTION,
    STRING = 0x03, //opcodes::STRING,
    HEADER = 0x04,
    END = 0x05
  };
  enum States state;
  enum States nextState;
  uint32_t pos;
  uint32_t spos;
  uint32_t lpos;
  uint32_t numSymbols;
  std::vector<uint32_t> symbols;
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
    this->readHeader();
  };
  States next(uint8_t * code, uint8_t * operand);
  void readHeader();
};

class CompilerSymbol {
public:
  uint32_t name;
  uint8_t type;
  std::vector<uint8_t> code;
  CompilerSymbol() : name(0), type(0) {};
};

class Compiler {
private:
  std::vector<CompilerSymbol *> symbols;
  Node::Partials * partials;
  std::map<std::string,int> partialSymbols;
  
  CompilerSymbol * _compile(Node * node);
  void _compileIn(Node * node, CompilerSymbol * symbol);
  std::vector<uint8_t> * _serialize(CompilerSymbol * main);
  
  CompilerSymbol * getSymbol();
public:
  std::vector<uint8_t> * compile(Node * node);
  std::vector<uint8_t> * compile(Node * node, Node::Partials * partials);
  
  static const char * opcodeName(uint8_t code);
  
  static std::string * print(uint8_t * codes, int length);
  static std::string * print(std::vector<uint8_t> * codes);
  
  static void bufferToVector(uint8_t * buffer, int length, std::vector<uint8_t> ** vector);
  static void stringToBuffer(std::string * string, uint8_t ** buffer, int * length);
  static void stringToVector(std::string * string, std::vector<uint8_t> ** vector);
  static void vectorToBuffer(std::vector<uint8_t> * vector, uint8_t ** buffer, int * length);
  
  static bool hasOperand(uint8_t code) {
    switch( code ) {
      case opcodes::FUNCTION:
      case opcodes::STRING:
      
      case opcodes::PRINTSYM:
      case opcodes::PRINTL:
      case opcodes::CALLSYM:
      case opcodes::CALL:
      case opcodes::JUMP:
      case opcodes::JUMPL:
      case opcodes::PUSH:
      
      case opcodes::DLOOKUPSYM:
      case opcodes::DLOOKUP:
      case opcodes::DLOOKUPNRSYM:
      case opcodes::DLOOKUPNR:
        
      case opcodes::CALLEXT:
        return true;
        break;
      default:
        return false;
    }
  };
};

}


#endif	/* MUSTACHE_COMPILER_HPP */
