
#include "vm.hpp"

#define PUSH(v) stack[stackSize++] = v
#define POP stackSize--
#define POPR stack[--stackSize]
#define TOP stack[stackSize - 1]
#define JUMP(pos) loc = codes + (i = pos);
#define SKIP ++loc, ++i

#ifdef DEBUG
#define DBG(...) printf(__VA_ARGS__)
#else
#define DBG(...) 
#endif

namespace mustache {

std::string * VM::execute(uint8_t * codes, int length, Data * data)
{
  std::string * output = new std::string;
  register uint8_t * loc = codes;
  register int i = 0;
  register int len = length;
  unsigned long stack[127];
  int stackSize = 0;
  DataStack * dataStack = new DataStack();
  dataStack->push_back(data);
  Data * current = dataStack->back();
  std::string lookupstr;
  
  for( ; i < length; i++, loc++ ) {
    // Scan to main
    if( *loc == 0x02 && *(loc + 1) == 0x00 ) {
      i += 2, loc += 2;
      break;
    }
  }
  
  // First thing to put on stack is code past the end of length so that the VM
  // will terminate when returning from main
  PUSH(length + 1);
  
  for( ; i < len; i++, loc++ ) {
    switch( *loc ) {
      case opcodes::CALL:
        DBG("%03d: CALL, Push: %ld, Jump: %u\n", i, loc - codes, *(1 + loc));
        PUSH(loc - codes + 1);
        JUMP(*(++loc) - 1);  // Need -1 so increment on loop will cancel out
        break;
      case opcodes::RETURN:
        DBG("%03d: RETURN, Jump: %lu\n", i, TOP);
        JUMP(POPR);
        break;
      case opcodes::PRINTL:
        DBG("%03d: PRINTL, Jump: %u\n", i, *(1 + loc));
        output->append((const /*unsigned*/ char *) (codes + *(++loc)));
        break;
      case opcodes::LOOKUP: {
        DBG("%03d: LOOKUP, Jump: %u", i, *(1 + loc));
        char * cstr = (/*unsigned*/ char *) (codes + *(++loc));
        lookupstr.assign(cstr);
        void * point = NULL;
        current = dataStack->back();
        if( current->type == Data::TypeMap ) {
          Data::Map::iterator it = current->data.find(lookupstr);
          if( it != current->data.end() ) {
            if( it->second->type == Data::TypeString ) {
              point = (void *) it->second;
            }
          }
        }
        DBG(", push: %p\n", point);
        PUSH((unsigned long) point);
        break;
      }
      case opcodes::PRINTD:
        DBG("%03d: PRINTD, Jump: %d\n", i, 0);
        current = (Data *) POPR;
        if( current != NULL && current->type == Data::TypeString ) {
          output->append(*current->val);
        }
        break;
      case opcodes::PRINTDE:
        DBG("%03d: PRINTD, Jump: %d\n", i, 0);
        current = (Data *) POPR;
        if( current != NULL && current->type == Data::TypeString ) {
          output->append(*current->val);
        }
        break;
      case opcodes::IF_EMPTY:
        DBG("%03d: IF_EMPTY, Jump: %d, Top %lu", i, 0, TOP);
        current = (Data *) POPR;
        if( current == NULL || current->isEmpty() ) {
          DBG(", empty");
        } else {
          DBG(", not empty");
          SKIP;
        }
        DBG("\n");
        break;
      case opcodes::IF_NOTEMPTY:
        DBG("%03d: IF_NOTEMPTY, Jump: %d, Top %lu", i, 0, TOP);
        current = (Data *) POPR;
        if( current != NULL && !current->isEmpty() ) {
          DBG(", not empty");
        } else {
          DBG(", empty");
          SKIP;
        }
        DBG("\n");
        break;
      case opcodes::NOOP:
        DBG("%03d: NOOP, Jump: %d\n", i, len + 1);
        JUMP(len + 1);
        break;
      default:
        DBG("%03d: UNKNOWN 0x%02x\n", i, *loc);
        break;
    }
  }
  
  return output;
}

}