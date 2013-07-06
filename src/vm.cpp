
#include "vm.hpp"

#define PUSH(v) stack[stackSize++] = v
#define POPN stackSize--
#define POPR stack[--stackSize]
#define TOP stack[stackSize - 1]
#define TOP1 stack[stackSize - 2]
#define JUMP(pos) loc = codes + pos
#define SKIP ++loc; if( Compiler::hasOperand(*loc) ) ++loc
#define LOC loc - codes

#define _DTOP dataStack->back()
#define _DPOP dataStack->pop_back()
#define _DPOPR dataStack->back(); dataStack->pop_back()
#define _DPUSH(v) dataStack->push_back(v)
#define _DSEARCH(v) dataStack->search(v)
#define _DSEARCHNR(v) dataStack->searchnr(v)

#ifdef DEBUG
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBG(...) 
#endif

namespace mustache {

std::string * VM::execute(uint8_t * codes, int length, Data * data)
{
  std::string * output = new std::string;
  execute(codes, length, data, output);
  return output;
}

void VM::execute(uint8_t * codes, int length, Data * data, std::string * output)
{
  register uint8_t * loc = codes;
  register uint8_t * end = codes + length;
  unsigned long stack[127];
  int stackSize = 0;
  DataStack * dataStack = new DataStack();
  dataStack->push_back(data);
  Data * current = dataStack->back();
  std::string lookupstr;
  
  for( ; loc < end; loc++ ) {
    // Scan to main
    if( *loc == 0x02 && *(loc + 1) == 0x00 ) {
      loc += 2;
      break;
    }
  }
  
  // First thing to put on stack is code past the end of length so that the VM
  // will terminate when returning from main
  PUSH(length + 1);
  
  for( ; loc < end; loc++ ) {
    switch( *loc ) {
      /** Basic operations -------------------------------------------------- */
      case opcodes::PRINTL:
        DBG("%03d: PRINTL, Jump: %u\n", LOC, *(1 + loc));
        output->append((const /*unsigned*/ char *) (codes + *(++loc)));
        break;
      case opcodes::CALL:
        DBG("%03d: CALL, Push: %ld, Jump: %u\n", LOC, loc - codes, *(1 + loc));
        PUSH(loc - codes + 1);
        JUMP(*(++loc) - 1);  // Need -1 so increment on loop will cancel out
        break;
      case opcodes::RETURN:
        DBG("%03d: RETURN, Jump: %lu\n", LOC, TOP);
        JUMP(POPR);
        break;
      case opcodes::JUMP:
        DBG("%03d: JUMP, Jump: %d\n", LOC, *(1 + loc));
        JUMP(*(++loc) - 1); // -1 to cancel out the next loop increment
        break;
      case opcodes::PUSH:
        DBG("%03d: PUSH\n", LOC);
        PUSH(*(++loc));
        break;
      case opcodes::POP:
        DBG("%03d: POP\n", LOC);
        POPN;
        break;
      case opcodes::INCR:
        DBG("%03d: INCR\n", LOC);
        TOP++;
        break;
      case opcodes::IF_GE:
        DBG("%03d: IF_GE, %ld >= %ld", LOC, TOP, TOP1);
        if( TOP >= TOP1 ) {
          DBG(", GE");
        } else {
          DBG(", !GE");
          SKIP;
        }
        DBG("\n");
        break;
      
      /** Data operations --------------------------------------------------- */
      case opcodes::DPUSH:
        DBG("%03d: DPUSH, Top %p\n", LOC, TOP);
        _DPUSH((Data *) TOP);
        break;
      case opcodes::DPOP:
        DBG("%03d: DPOP\n", LOC);
        _DPOP;
        break;
      case opcodes::DLOOKUP: {
        DBG("%03d: DLOOKUP, Symbol: %u", LOC, *(1 + loc));
        char * cstr = (/*unsigned*/ char *) (codes + *(++loc));
        DBG(", String: %s", cstr);
        lookupstr.assign(cstr);
        DBG(", DPush: %p\n", _DSEARCH(&lookupstr));
        _DPUSH(_DSEARCH(&lookupstr));
        break;
      }
      case opcodes::DLOOKUPNR: {
        DBG("%03d: DLOOKUPNR, Symbol: %u", LOC, *(1 + loc));
        char * cstr = (/*unsigned*/ char *) (codes + *(++loc));
        DBG(", String: %s", cstr);
        lookupstr.assign(cstr);
        DBG(", DPush: %p\n", _DSEARCHNR(&lookupstr));
        _DPUSH(_DSEARCHNR(&lookupstr));
        break;
      }
      case opcodes::DLOOKUPA: {
        DBG("%03d: DLOOKUP, Index: %d", LOC, TOP);
        DBG(", DPush: %p\n", _DTOP->array + TOP);
        _DPUSH(_DTOP->array + TOP);
        break;
      }
      case opcodes::DARRSIZE:
        DBG("%03d: DARRSIZE", LOC);
        if( _DTOP != NULL && _DTOP->type == Data::TypeArray ) {
          DBG(", Size: %d", _DTOP->length);
          PUSH(_DTOP->length);
        } else {
          DBG(", Size: %d", 0);
          PUSH(0);
        }
        DBG("\n");
        break;
      case opcodes::DPRINT:
        DBG("%03d: DPRINT, DTop: %p", LOC, _DTOP);
        if( _DTOP != NULL && _DTOP->type == Data::TypeString ) {
          output->append(*(_DTOP->val));
        }
        _DPOP;
        DBG("\n");
        break;
      case opcodes::DPRINTE:
        DBG("%03d: DPRINTE, DTop: %p", LOC, _DTOP);
        if( _DTOP != NULL && _DTOP->type == Data::TypeString ) {
          htmlspecialchars_append((_DTOP->val), output);
        }
        _DPOP;
        DBG("\n");
        break;
      
      /** Data conditionals ------------------------------------------------- */
      case opcodes::DIF_EMPTY:
        DBG("%03d: DIF_EMPTY, DTop %p", LOC, _DTOP);
        if( _DTOP == NULL || _DTOP->isEmpty() ) {
          DBG(", empty");
        } else {
          DBG(", not empty");
          SKIP;
        }
        DBG("\n");
        break;
      case opcodes::DIF_NOTEMPTY:
        DBG("%03d: DIF_NOTEMPTY, DTop %p", LOC, _DTOP);
        if( _DTOP != NULL && !_DTOP->isEmpty() ) {
          DBG(", not empty");
        } else {
          DBG(", empty");
          SKIP;
        }
        DBG("\n");
        break;
      case opcodes::DIF_HASH:
        DBG("%03d: DIF_HASH, DTop %p", LOC, _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeMap ) {
          DBG(", not hash");
          SKIP;
        } else {
          DBG(", hash");
        }
        DBG("\n");
        break;
      case opcodes::DIF_NOTHASH:
        DBG("%03d: DIF_NOTHASH, DTop %p", LOC, _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeMap ) {
          DBG(", not hash");
        } else {
          DBG(", hash");
          SKIP;
        }
        DBG("\n");
        break;
      case opcodes::DIF_ARRAY:
        DBG("%03d: DIF_ARRAY, DTop %p", LOC, _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeArray ) {
          DBG(", not array");
          SKIP;
        } else {
          DBG(", array");
        }
        DBG("\n");
        break;
      case opcodes::DIF_NOTARRAY:
        DBG("%03d: DIF_NOTARRAY, DTop %p", LOC, _DTOP);
        if( _DTOP == NULL || _DTOP->type != Data::TypeArray ) {
          DBG(", not array");
        } else {
          DBG(", array");
          SKIP;
        }
        DBG("\n");
        break;
      
      /** Unknown ----------------------------------------------------------- */
      case opcodes::NOOP:
        DBG("%03d: NOOP, Jump: %d\n", LOC, length + 1);
        JUMP(length + 1);
        break;
      default:
        DBG("%03d: UNKNOWN 0x%02x\n", LOC, *loc);
        break;
    }
  }
}

}