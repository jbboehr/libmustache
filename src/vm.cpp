
#include "vm.hpp"

#define JUMP(pos) loc = codes + pos
#define SKIP ++loc; if( Compiler::hasOperand(*loc) ) ++loc
#define RUN loc = codes + length + 1
#define LOC loc - codes
#define LLOC loc - sloc

#define _EPUSH(v) stack[stackSize++] = v
#define _EPOP stack[--stackSize]
#define _ETOP stack[stackSize - 1]
#define _ETOP1 stack[stackSize - 2]

#define _DPUSH(v) dataStack[dataStackSize++] = v
#define _DPOP dataStack[--dataStackSize]
#define _DTOP dataStack[dataStackSize - 1]
#define _DSEARCH(v) this->search(dataStackSize, v)
#define _DSEARCHNR(v) this->searchnr(dataStackSize, v)

#ifdef DEBUG
#define DBGHEAD printf("%03ld: %03ld: %-8s\t", LOC, LLOC, Compiler::opcodeName(*loc))
#define DBG(...) printf(__VA_ARGS__)
#define DBGFOOT printf("\n");
#else
#define DBGHEAD
#define DBG(...)
#define DBGFOOT
#endif

namespace mustache {

std::string * VM::execute(uint8_t * codes, size_t length, Data * data)
{
  std::string * output = new std::string;
  execute(codes, length, data, output);
  return output;
}

void VM::execute(std::vector<uint8_t> codes, Data * data, std::string * output)
{
#ifdef HAVE_CXX11
  execute(codes.data(), codes.size(), data, output);
#else
  uint8_t * codes_buf = NULL;
  int codes_length = 0;
  Compiler::vectorToBuffer(codes, &codes_buf, &codes_length);
  execute(codes_buf, codes_length, data, output);
  free(codes_buf);
#endif
}

void VM::execute(uint8_t * codes, size_t length, Data * data, std::string * output)
{
  register uint8_t * loc = codes;
  register uint8_t * end = codes + length;
  register uint8_t * sloc = loc;
  register uint32_t * symbols = NULL;
  register uint32_t * stack = this->stack;
  register uint32_t stackSize = 0;
  register Data ** dataStack = this->dataStack;
  register uint32_t dataStackSize = 0;
  
  // Clear the output buffer
  outputBuffer.clear();
  
  // Push the initial data onto the data stack
  _DPUSH(data);
  
  // Read the symbol table
  {
    uint32_t nSymbols = _UNPACK4A(loc, 0);
    uint32_t i;
    loc += 4;
    symbols = (uint32_t *) malloc(sizeof(uint32_t) * nSymbols);
    for( i = 0; i < nSymbols; i++ ) {
      symbols[i] = _UNPACK4A(loc, 0);
      DBG("Symbol %d @ %u\n", i, symbols[i]);
      loc += 4;
    }
  }
  
  // Jump to main
  sloc = loc = codes + symbols[0] + 2;
  
  // First thing to put on stack is code past the end of length so that the VM
  // will terminate when returning from main
  _EPUSH(LOC);
  _EPUSH(length + 1);
  
  for( ; loc < end; loc++ ) {
    DBGHEAD;
    switch( *loc ) {
      /** Basic operations -------------------------------------------------- */
      case opcodes::PRINTL:
        DBG("Jump: %u", *(1 + loc));
        outputBuffer.append((const char *) (codes + *(++loc)));
        break;
      case opcodes::PRINTSYM:
        DBG("Symbol: %u, Jump: %u", *(1 + loc), symbols[*(1 + loc)]);
        outputBuffer.append((const char *) (codes + symbols[*(++loc)] + 2)); // Need 2 to skip symbol name and type marker
        break;
      case opcodes::CALL:
        DBG("Push: %ld, Jump: %u", loc - codes, *(1 + loc));
        _EPUSH(sloc - codes);
        _EPUSH(loc - codes + 1);
        JUMP(*(++loc) - 1);  // Need -1 so increment on loop will cancel out
        sloc = loc + 1;
        break;
      case opcodes::CALLSYM:
        DBG("Symbol: %d, Push: %ld, Jump: %u", *(1 + loc), loc - codes, symbols[*(1 + loc)]);
        _EPUSH(sloc - codes);
        _EPUSH(loc - codes + 1);
        JUMP(symbols[*(++loc)] + 2 - 1);  // Need -1 so increment on loop will cancel out
        sloc = loc + 1;
        break;
      case opcodes::RETURN:
        DBG("Jump: %lu", _ETOP);
        JUMP(_EPOP);
        sloc = codes + _EPOP;
        break;
      case opcodes::JUMP:
        DBG("Jump: %u", *(1 + loc));
        JUMP(*(++loc) - 1); // -1 to cancel out the next loop increment
        break;
      case opcodes::JUMPL:
        DBG("Jump: %u", (sloc - codes) + *(1 + loc));
        JUMP((sloc - codes) + *(++loc) - 1); // -1 to cancel out the next loop increment
        break;
      case opcodes::PUSH:
        DBG("Push: %u", *(1 + loc));
        _EPUSH(*(++loc));
        break;
      case opcodes::POP:
        DBG("Top: %lu", _ETOP);
        _EPOP;
        break;
      case opcodes::INCR:
        DBG("%lu+1", _ETOP);
        _ETOP++;
        break;
      case opcodes::IF_GE:
        DBG("%ld >= %ld", _ETOP, _ETOP1);
        if( _ETOP >= _ETOP1 ) {
          DBG(", GE");
        } else {
          DBG(", !GE");
          SKIP;
        }
        break;
      
      /** Data operations --------------------------------------------------- */
//      case opcodes::DPUSH:
//        DBG("Top %p", (Data *) _ETOP);
//        _DPUSH((Data *) _ETOP);
//        break;
      case opcodes::DPOP:
        DBG("DTop: %p", (Data *) _DTOP);
        _DPOP;
        break;
      case opcodes::DLOOKUP: {
        DBG("Symbol: %u", *(1 + loc));
        const char * cstr = (const char *) (codes + *(++loc));
        DBG(", String: %s", cstr);
        lookupstr.assign(cstr);
        DBG(", DPush: %p", _DSEARCH(&lookupstr));
        _DPUSH(_DSEARCH(&lookupstr));
        break;
      }
      case opcodes::DLOOKUPSYM: {
        DBG("Symbol: %u, Jump: %d", *(1 + loc), symbols[*(1 + loc)]);
        const char * cstr = (const char *) (codes + symbols[*(++loc)] + 2); // Need 2 to skip symbol name and type
        DBG(", String: %s", cstr);
        lookupstr.assign(cstr);
        DBG(", DPush: %p", _DSEARCH(&lookupstr));
        _DPUSH(_DSEARCH(&lookupstr));
        break;
      }
      case opcodes::DLOOKUPNR: {
        DBG("Symbol: %u", *(1 + loc));
        char * cstr = (/*unsigned*/ char *) (codes + *(++loc));
        DBG(", String: %s", cstr);
        lookupstr.assign(cstr);
        DBG(", DPush: %p", _DSEARCHNR(&lookupstr));
        _DPUSH(_DSEARCHNR(&lookupstr));
        break;
      }
      case opcodes::DLOOKUPNRSYM: {
        DBG("Symbol: %u, Jump: %d", *(1 + loc), symbols[*(1 + loc)]);
        const char * cstr = (const char *) (codes + symbols[*(++loc)] + 2); // Need 2 to skip symbol name and type
        DBG(", String: %s", cstr);
        lookupstr.assign(cstr);
        DBG(", DPush: %p", _DSEARCHNR(&lookupstr));
        _DPUSH(_DSEARCHNR(&lookupstr));
        break;
      }
      case opcodes::DLOOKUPA: {
        DBG("Index: %lu, DPush: %p", _ETOP, _DTOP->array + _ETOP);
        _DPUSH(_DTOP->array + _ETOP);
        break;
      }
      case opcodes::DARRSIZE:
        if( _DTOP != NULL && _DTOP->type == Data::TypeArray ) {
          _EPUSH(_DTOP->length);
        } else {
          _EPUSH(0);
        }
        DBG("Size: %lu", _ETOP);
        break;
      case opcodes::DPRINT:
        DBG("DTop: %p", _DTOP);
        if( _DTOP != NULL && _DTOP->type == Data::TypeString ) {
          outputBuffer.append(*(_DTOP->val));
        }
        break;
      case opcodes::DPRINTE:
        DBG("DTop: %p", _DTOP);
        if( _DTOP != NULL && _DTOP->type == Data::TypeString ) {
          htmlspecialchars_append((_DTOP->val), &outputBuffer);
        }
        break;
      
      /** Data conditionals ------------------------------------------------- */
      case opcodes::DIF_EMPTY:
        DBG("DTop %p", _DTOP);
        if( _DTOP == NULL || _DTOP->isEmpty() ) {
          DBG(", empty");
        } else {
          DBG(", not empty");
          SKIP;
        }
        break;
      case opcodes::DIF_NOTEMPTY:
        DBG("DTop %p", _DTOP);
        if( _DTOP != NULL && !_DTOP->isEmpty() ) {
          DBG(", not empty");
        } else {
          DBG(", empty");
          SKIP;
        }
        break;
      case opcodes::DIF_HASH:
        DBG("DTop %p", _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeMap ) {
          DBG(", not hash");
          SKIP;
        } else {
          DBG(", hash");
        }
        break;
      case opcodes::DIF_NOTHASH:
        DBG("DTop %p", _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeMap ) {
          DBG(", not hash");
        } else {
          DBG(", hash");
          SKIP;
        }
        break;
      case opcodes::DIF_ARRAY:
        DBG("DTop %p", _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeArray ) {
          DBG(", not array");
          SKIP;
        } else {
          DBG(", array");
        }
        break;
      case opcodes::DIF_NOTARRAY:
        DBG("DTop %p", _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeArray ) {
          DBG(", not array");
        } else {
          DBG(", array");
          SKIP;
        }
        break;
      
      /** Unknown ----------------------------------------------------------- */
      case opcodes::CALLEXT:
        DBG("NOT IMPLEMENTED %d\n", *(++loc));
        break;
      case opcodes::NOOP:
        DBG("Jump: %d", length + 1);
        JUMP(length + 1);
        break;
      default:
        DBG("UNKNOWN 0x%02x\n", *loc);
        break;
    }
    DBGFOOT;
  }
  
  // Copy the internal output buffer to the output
  output->assign(outputBuffer);
  
  // Free
  free(symbols);
}

Data * VM::search(uint32_t dataStackSize, std::string * key)
{
  // Resolve up the data stack
  Data * ref = NULL;
  Data::Map::iterator d_it;
  register Data ** _stackPos = this->dataStack + dataStackSize - 1;
  register int i;
  for( i = 0; i < dataStackSize; i++, _stackPos-- ) {
    if( (*_stackPos) == NULL ) continue;
    if( (*_stackPos)->type != Data::TypeMap ) continue;
    
    d_it = (*_stackPos)->data.find(*key);
    if( d_it != (*_stackPos)->data.end() ) {
      ref = d_it->second;
      if( ref != NULL ) {
        break;
      }
    }
  }
  return ref;
}

Data * VM::searchnr(uint32_t dataStackSize, std::string * key)
{
  Data * back = this->dataStack[dataStackSize - 1];
  if( back == NULL || back->type != Data::TypeMap ) {
    return NULL;
  }
  
  Data * ref = NULL;
  Data::Map::iterator d_it = back->data.find(*key);
  if( d_it != back->data.end() ) {
    ref = d_it->second;
    if( ref != NULL ) {
      return ref;
    }
  }
  
  return NULL;
}

}
