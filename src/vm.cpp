
#include "vm.hpp"

#define PUSH(v) stack[stackSize++] = v
#define POPN --stackSize
#define POPR stack[--stackSize]
#define TOP stack[stackSize - 1]
#define TOP1 stack[stackSize - 2]
#define JUMP(pos) loc = codes + pos
#define SKIP ++loc; if( Compiler::hasOperand(*loc) ) ++loc
#define LOC loc - codes
#define LLOC loc - sloc

#define _DTOP dataStack->back()
#define _DPOP dataStack->pop_back()
#define _DPOPR dataStack->back(); dataStack->pop_back()
#define _DPUSH(v) dataStack->push_back(v)
#define _DSEARCH(v) dataStack->search(v)
#define _DSEARCHNR(v) dataStack->searchnr(v)

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
  register uint8_t * sloc = loc;
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
  sloc = loc;
  
  // First thing to put on stack is code past the end of length so that the VM
  // will terminate when returning from main
  PUSH(length + 1);
  
  for( ; loc < end; loc++ ) {
    DBGHEAD;
    switch( *loc ) {
      /** Basic operations -------------------------------------------------- */
      case opcodes::PRINTL:
        DBG("Jump: %u", *(1 + loc));
        output->append((const char *) (codes + *(++loc)));
        break;
      case opcodes::CALL:
        DBG("Push: %ld, Jump: %u", loc - codes, *(1 + loc));
        PUSH(sloc - codes);
        PUSH(loc - codes + 1);
        JUMP(*(++loc) - 1);  // Need -1 so increment on loop will cancel out
        sloc = loc + 1;
        break;
      case opcodes::RETURN:
        DBG("Jump: %lu", TOP);
        JUMP(POPR);
        sloc = codes + POPR;
        break;
      case opcodes::JUMP:
        DBG("Jump: %u", *(1 + loc));
        JUMP(*(++loc) - 1); // -1 to cancel out the next loop increment
        break;
      case opcodes::PUSH:
        DBG("Push: %u", *(1 + loc));
        PUSH(*(++loc));
        break;
      case opcodes::POP:
        DBG("Top: %lu", TOP);
        POPN;
        break;
      case opcodes::INCR:
        DBG("%lu+1", TOP);
        TOP++;
        break;
      case opcodes::IF_GE:
        DBG("%ld >= %ld", TOP, TOP1);
        if( TOP >= TOP1 ) {
          DBG(", GE");
        } else {
          DBG(", !GE");
          SKIP;
        }
        break;
      
      /** Data operations --------------------------------------------------- */
      case opcodes::DPUSH:
        DBG("Top %p", (Data *) TOP);
        _DPUSH((Data *) TOP);
        break;
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
      case opcodes::DLOOKUPNR: {
        DBG("Symbol: %u", *(1 + loc));
        char * cstr = (/*unsigned*/ char *) (codes + *(++loc));
        DBG(", String: %s", cstr);
        lookupstr.assign(cstr);
        DBG(", DPush: %p", _DSEARCHNR(&lookupstr));
        _DPUSH(_DSEARCHNR(&lookupstr));
        break;
      }
      case opcodes::DLOOKUPA: {
        DBG("Index: %lu, DPush: %p", TOP, _DTOP->array + TOP);
        _DPUSH(_DTOP->array + TOP);
        break;
      }
      case opcodes::DARRSIZE:
        if( _DTOP != NULL && _DTOP->type == Data::TypeArray ) {
          PUSH(_DTOP->length);
        } else {
          PUSH(0);
        }
        DBG("Size: %lu", TOP);
        break;
      case opcodes::DPRINT:
        DBG("DTop: %p", _DTOP);
        if( _DTOP != NULL && _DTOP->type == Data::TypeString ) {
          output->append(*(_DTOP->val));
        }
        _DPOP;
        break;
      case opcodes::DPRINTE:
        DBG("DTop: %p", _DTOP);
        if( _DTOP != NULL && _DTOP->type == Data::TypeString ) {
          htmlspecialchars_append((_DTOP->val), output);
        }
        _DPOP;
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
}

}