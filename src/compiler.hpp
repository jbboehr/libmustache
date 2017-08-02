
#ifndef MUSTACHE_COMPILER_HPP
#define MUSTACHE_COMPILER_HPP

#include "mustache_config.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#ifdef _MSC_VER
#define snprintf _snprintf_s
#endif

#include "node.hpp"
#include "exception.hpp"

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
#define _UNPACK2A(a, k) (_UNPACKI(a[k], 8) + _UNPACK1A(a, k + 1))
#define _UNPACK3A(a, k) (_UNPACKI(a[k], 16) + _UNPACK2A(a, k + 1))
#define _UNPACK4A(a, k) (_UNPACKI(a[k], 24) + _UNPACK3A(a, k + 1))

#if MUSTACHE_VM_OP_SIZE == 1
typedef uint8_t _C_OP_TYPE;
#define _C_OP_SIZE 1
#define _C_OP_PACKFN _PACK1FN
#define _C_OP_PACKA _PACK1A
#define _C_OP_UNPACKA _UNPACK1A
#elif MUSTACHE_VM_OP_SIZE == 2 || !defined(MUSTACHE_VM_OP_SIZE)
typedef uint16_t _C_OP_TYPE;
#define _C_OP_SIZE 2
#define _C_OP_PACKFN _PACK2FN
#define _C_OP_PACKA _PACK2A
#define _C_OP_UNPACKA _UNPACK2A
#elif MUSTACHE_VM_OP_SIZE == 3
typedef uint32_t _C_OP_TYPE;
#define _C_OP_SIZE 3
#define _C_OP_PACKFN _PACK3FN
#define _C_OP_PACKA _PACK3A
#define _C_OP_UNPACKA _UNPACK3A
#elif MUSTACHE_VM_OP_SIZE == 4
typedef uint32_t _C_OP_TYPE;
#define _C_OP_SIZE 4
#define _C_OP_PACKFN _PACK4FN
#define _C_OP_PACKA _PACK4A
#define _C_OP_UNPACKA _UNPACK4A
#else
#error "Invalid operand size"
#endif

#define _CPUSH(vect, code) vect.push_back(code)
#define _CPUSHOP(vect, code, operand) \
    do { \
      vect.push_back(code); \
      _C_OP_PACKFN(vect.push_back, operand); \
    } while(0)
#define _CSET(vect, index, code) vect[index] = code
#define _CSETOP(vect, index, code) _C_OP_PACKA(vect, index, code)
#define _CLEN(vect) vect.size()
#define _CLENP(vect) _CLEN(vect) - _C_OP_SIZE

namespace mustache {


namespace opcodes {
/**
 * Enumeration of opcodes
 */
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
  PRINTSYM = 0x04,
  
  /**
   * Prints char * in codes point of the operand.
   */
  PRINTL = 0x05,
  
  /**
   * Call the function symbol specified by the operand. The current point in 
   * execution is pushed onto the execution stack
   */
  CALLSYM = 0x06,
  
  /**
   * Call the function specified by the operand. The current point in execution
   * is pushed onto the execution stack
   */
  CALL = 0x07,
  
  /**
   * Return from a function. The current execution point is popped from the top
   * of the stack
   */
  RETURN = 0x08,
  
  /**
   * Jump to the operand
   */
  JUMP = 0x09,
  
  /**
   * Jump to the local position by the operand. Translated into jump
   */
  JUMPL = 0x0a,
  
  /**
   * Push the operand onto the execution stack
   */
  PUSH = 0x0b,
  
  /**
   * Pop the top of the execution stack
   */
  POP = 0x0c,
  
  /**
   * Increment the top of the execution stack
   */
  INCR = 0x0d,
  
  /**
   * If the top of the execution stack is greater than top - 1, then execute
   * the next operation
   */
  IF_GE = 0x0e,
  
  
  /** Data operations ------------------------------------------------------- */
  
  /**
   * Push the top of the execution stack onto the data stack
   */
  DPUSH = 0x0f,
  
  /**
   * Pop the top of the data stack
   */
  DPOP = 0x10,
  
  /**
   * Pushes the location of the hash value from the hash on the top of the 
   * data stack specified by the symbol of the operand onto the data stack.
   */
  DLOOKUPSYM = 0x11,
  
  /**
   * Translated version of DLOOKUPSYM
   */
  DLOOKUP = 0x12,
  
  /**
   * Pushes the location of the hash value from the hash on the top of the 
   * data stack specified by the symbol of the operand onto the data stack.
   * Does not recurse up the data stack
   */
  DLOOKUPNRSYM = 0x13,
  
  /**
   * Translated version of DLOOKUPNRSYM
   */
  DLOOKUPNR = 0x14,
  
  /**
   * Look up the array key in the data stack specified by the top of the 
   * execution stack and push it on the top of the data stack
   */
  DLOOKUPA = 0x15,
  
  /**
   * Push the size of the array on the top of the data stack onto the 
   * execution stack
   */
  DARRSIZE = 0x16,
  
  /**
   * Prints the string data on the top of the data stack
   */
  DPRINT = 0x17,
  
  /**
   * Prints and escapes the string data on the top of the data stack
   */
  DPRINTE = 0x18,
  
  
  
  /** Data conditionals ----------------------------------------------------- */
  
  /**
   * If the data pointed to by the top of the data stack is empty,
   * execute the next operation, otherwise skip
   */
  DIF_EMPTY = 0x19,
          
  /**
   * If the data pointed to by the top of the data stack is not empty,
   * execute the next operation, otherwise skip
   */
  DIF_NOTEMPTY = 0x1a,
  
  /**
   * If the data pointed to by the top of the data stack is a hash,
   * execute the next operation
   */
  DIF_HASH = 0x1b,
  
  /**
   * If the data pointed to by the top of the data stack is a hash,
   * execute the next operation
   */
  DIF_NOTHASH = 0x1c,
  
  /**
   * If the data pointed to by the top of the data stack is an array,
   * execute the next operation
   */
  DIF_ARRAY = 0x1d,
  
  /**
   * If the data pointed to by the top of the data stack is not an array,
   * execute the next operation
   */
  DIF_NOTARRAY = 0x1e,
  
  
  
  
  /**
   * If the data pointed to by the top of the data stack is not an array,
   * execute the next operation
   */
  CALLEXT = 0x1f,
  
} opcodes;
} // namespace opcodes

namespace CompilerStates {
  typedef enum States {
    NOOP = 0x00, //opcodes::NOOP,
    SYMBOL = 0x01,  //opcodes::SYMBOL,
    FUNCTION = 0x02, //opcodes::FUNCTION,
    STRING = 0x03, //opcodes::STRING,
    HEADER = 0x04,
    END = 0x05
  } States;
}


/**
 * Bytecode parser. Not optimized for speed, only use in the compiler, 
 * not the VM
 */
class CompilerState {
public:
  enum CompilerStates::States state;
  enum CompilerStates::States nextState;
  uint32_t pos;
  uint32_t spos;
  uint32_t lpos;
  uint32_t numSymbols;
  std::vector<uint32_t> symbols;
  std::vector<uint8_t> * codes;
  CompilerState() : 
        state(CompilerStates::NOOP),
        pos(0),
        codes(NULL) {
    ;
  };
  CompilerState(std::vector<uint8_t> * codes) : 
        state(CompilerStates::NOOP),
        pos(0) {
    this->state = state;
    this->pos = pos;
    this->codes = codes;
    this->readHeader();
  };
  int next(uint8_t * code, _C_OP_TYPE * operand);
  void readHeader();
};



/**
 * Compiler symbol
 */
class CompilerSymbol {
public:
  //! The name of the symbol
  uint32_t name;
  
  //! The type of the symbol (see opcodes 2-3)
  uint8_t type;
  
  //! Vector of the symbol code
  std::vector<uint8_t> code;
  
  //! Constructor
  CompilerSymbol() : name(0), type(0) {};
};



/**
 * Compiler
 */
class Compiler {
private:
  //! Stores the symbols during compilation
  std::vector<CompilerSymbol *> symbols;
  
  //! Stores any partials to be compiled into the current code
  Node::Partials * partials;
  
  //! Stores the symbol names for partials to be compiled in
  std::map<std::string,int> partialSymbols;
  
  //! Build the context shift bytecode for a node that causes the context to shift
  int _makeLookup(Node * node, CompilerSymbol * sym);
  
  //! Build the context shift cleanup bytecode
  void _makeLookupEnd(int num, CompilerSymbol * sym);
  
  //! Compile a node
  CompilerSymbol * _compile(Node * node);
  
  //! Compile a node
  void _compile(Node * node, CompilerSymbol * sym);
  
  //! Serialize all symbols into contiguous bytecode
  std::vector<uint8_t> * _serialize(CompilerSymbol * main);
  
  //! Get a symbol
  CompilerSymbol * getSymbol();
  
public:
  //! Compile a node
  std::vector<uint8_t> * compile(Node * node);
  
  //! Compile a node (with partials)
  std::vector<uint8_t> * compile(Node * node, Node::Partials * partials);
  
  //! Get the name of an opcode
  static const char * opcodeName(uint8_t code);
  
  //! Print readable form of bytecode
  static std::string * print(uint8_t * codes, int length);
  
  //! Print readable form of bytecode
  static std::string * print(std::vector<uint8_t> * codes);
  
  //! Converts a uint8_t buffer to a uint8_t vector
  static void bufferToVector(uint8_t * buffer, size_t length, std::vector<uint8_t> ** vector);
  
  //! Converts a string to a uint8_t buffer
  static void stringToBuffer(std::string * string, uint8_t ** buffer, size_t * length);
  
  //! Converts a string to a uint8_t vector
  static void stringToVector(std::string * string, std::vector<uint8_t> ** vector);
  
  //! Converts a uint8_t vector to a uint8_t buffer
  static void vectorToBuffer(std::vector<uint8_t> * vector, uint8_t ** buffer, size_t * length);
  
  //! Does the opcode have an operand
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
