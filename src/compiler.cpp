
#include "compiler.hpp"

namespace mustache {


CompilerState::States CompilerState::next(uint8_t * code, uint8_t * operand) {
    if( this->pos > this->codes->size() ) {
      return this->state = States::END;
    }
    this->spos = this->pos;
    uint8_t * current = &((*this->codes)[this->pos]);
    uint8_t * next = &((*this->codes)[this->pos + 1]);
    switch( this->state ) {
      case States::NOOP:
        if( *current == opcodes::FUNCTION ) {
          this->state = States::SYMBOL;
          this->nextState = States::FUNCTION;
          *code = *current;
          *operand = *next;
          this->pos++; // Skip operand
        } else if( *current == opcodes::STRING ) {
          this->state = States::SYMBOL;
          this->nextState = States::STRING;
          *code = *current;
          *operand = *next;
          this->pos++; // Skip operand
        } else {
          this->state = States::NOOP;
          *code = *current;
          *operand = 0;
        }
        this->lpos = 0;
        break;
      case States::SYMBOL:
        *code = *current;
        *operand = *next;
        this->state = this->nextState;
        if( this->state == States::FUNCTION ) {
          this->pos++;
        }
        this->lpos = 0;
        break;
      case States::STRING:
        *code = *current;
        *operand = 0;
        if( *current == opcodes::NOOP ) {
          this->state = States::NOOP;
        }
        this->lpos++;
        break;
      case States::FUNCTION:
        if( *current == opcodes::NOOP ) {
          this->state = States::NOOP;
          *code = *current;
          *operand = 0;
        } else if( true == Compiler::hasOperand(*current) ) {
          *code = *current;
          *operand = *next;
          this->pos++;
          this->lpos++;
        } else {
          *code = *current;
          *operand = 0;
        }
        this->lpos++;
        break;
    }
    this->pos++;
    return this->state;
}

CompilerSymbol * Compiler::_compile(Node * node)
{
  CompilerSymbol * sym = NULL;
  CompilerSymbol * child = NULL;
  
  switch( node->type ) {
    case Node::TypeRoot: {
      sym = getSymbol();
      sym->type = mustache::opcodes::FUNCTION;
      if( node->children.size() > 0 ) {
        Node::Children::iterator it;
        for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
          if( (*it)->type & Node::TypeHasChildren ) {
            child = _compile(*it);
            sym->code.push_back(opcodes::CALLSYM);
            sym->code.push_back(child->name);
          } else {
            _compileIn(*it, sym);
          }
        }
      }
      sym->code.push_back(opcodes::RETURN);
      break;
    }
    case Node::TypeSection: {
      // Don't have to do anything if empty
      if( node->children.size() <= 0 ) {
        return NULL;
      }
      
      // Get the main symbol
      sym = getSymbol();
      sym->type = mustache::opcodes::FUNCTION;
      
      // Make the section name symbol
      CompilerSymbol * nameSym = getSymbol();
      nameSym->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        nameSym->code.push_back((uint8_t) node->data->at(i));
      }
      
      // Make the section children function symbol
      CompilerSymbol * childrenSym = getSymbol();
      childrenSym->type = mustache::opcodes::FUNCTION;
      Node::Children::iterator it;
      for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
        if( (*it)->type & Node::TypeHasChildren ) {
          child = _compile(*it);
          if( child != NULL ) {
            childrenSym->code.push_back(opcodes::CALLSYM);
            childrenSym->code.push_back(child->name);
          }
        } else {
          _compileIn(*it, childrenSym);
        }
      }
      childrenSym->code.push_back(opcodes::RETURN);
      
      // Make the main symbol
      sym->code.push_back(opcodes::DLOOKUPSYM);
      sym->code.push_back(nameSym->name);
      
      sym->code.push_back(opcodes::DIF_EMPTY);
      sym->code.push_back(opcodes::JUMPL);
      size_t ifemptyoppos = sym->code.size();
      sym->code.push_back(0x00);
      
      // If array, jump to the array loop
      sym->code.push_back(opcodes::DIF_ARRAY);
      sym->code.push_back(opcodes::JUMPL);
      size_t ifarrayoppos = sym->code.size();
      sym->code.push_back(0x00);
      
      // If not array, just call then jump to end
      sym->code.push_back(opcodes::CALLSYM);
      sym->code.push_back(childrenSym->name);
      sym->code.push_back(opcodes::JUMPL);
      size_t ifnotarrayoppos = sym->code.size();
      sym->code.push_back(0x00);
      
      // Do the array looping code
      sym->code[ifarrayoppos] = sym->code.size();
      sym->code.push_back(opcodes::DARRSIZE);
      sym->code.push_back(opcodes::PUSH);
      sym->code.push_back(0);
      size_t ifarrayinoppos = sym->code.size();
      
      sym->code.push_back(opcodes::IF_GE);
      sym->code.push_back(opcodes::JUMPL);
      size_t ifarrayendoppos = sym->code.size();
      sym->code.push_back(0x00);
      
      sym->code.push_back(opcodes::DLOOKUPA);
      
      sym->code.push_back(opcodes::CALLSYM);
      sym->code.push_back(childrenSym->name);
      
      sym->code.push_back(opcodes::DPOP);
      
      sym->code.push_back(opcodes::DIF_NOTARRAY);
      sym->code.push_back(opcodes::JUMPL);
      size_t ifnotarrayoppos2 = sym->code.size();
      sym->code.push_back(0x00);
      
      sym->code.push_back(opcodes::INCR);
      sym->code.push_back(opcodes::JUMPL);
      sym->code.push_back(ifarrayinoppos);
      
      sym->code[ifarrayendoppos] = sym->code.size();
      sym->code.push_back(opcodes::POP);
      sym->code.push_back(opcodes::POP);
      
      sym->code[ifemptyoppos] = sym->code.size();
      sym->code[ifnotarrayoppos] = sym->code.size();
      sym->code[ifnotarrayoppos2] = sym->code.size();
      sym->code.push_back(opcodes::DPOP);
      
      sym->code.push_back(opcodes::RETURN);
      break;
    }
    case Node::TypeNegate: {
      sym = getSymbol();
      sym->type = mustache::opcodes::FUNCTION;
      
      CompilerSymbol * nameSym = getSymbol();
      nameSym->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        nameSym->code.push_back((uint8_t) node->data->at(i));
      }
      
      sym->code.push_back(opcodes::DLOOKUPSYM);
      sym->code.push_back(nameSym->name);
      
      sym->code.push_back(opcodes::DIF_NOTEMPTY);
      sym->code.push_back(opcodes::JUMPL);
      size_t ifemptyoppos = sym->code.size();
      sym->code.push_back(0x00);
      
      if( node->children.size() > 0 ) {
        Node::Children::iterator it;
        for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
          if( (*it)->type & Node::TypeHasChildren ) {
            child = _compile(*it);
            sym->code.push_back(opcodes::CALLSYM);
            sym->code.push_back(child->name);
          } else {
            _compileIn(*it, sym);
          }
        }
      }
      
      sym->code[ifemptyoppos] = sym->code.size();
      sym->code.push_back(opcodes::DPOP);
      
      sym->code.push_back(opcodes::RETURN);
    }
  }
  
  return sym;
}

void Compiler::_compileIn(Node * node, CompilerSymbol * symbol)
{
  switch( node->type ) {
    case Node::TypeOutput: {
      // Store string in symbol
      CompilerSymbol * str = getSymbol();
      str->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        str->code.push_back((uint8_t) node->data->at(i));
      }
      //str->code.push_back(0x00); // The "linker" will null-terminate the string
      // Add print for symbol
      symbol->code.push_back(opcodes::PRINTSYM);
      symbol->code.push_back(str->name & 0xff); // @todo multi-byte
      break;
    }
    case Node::TypeVariable:
    case Node::TypeTag: {
      // Without dots
      if( node->dataParts == NULL ) {
        // Store string in symbol
        CompilerSymbol * str = getSymbol();
        str->type = mustache::opcodes::STRING;
        for( int i = 0, l = node->data->length(); i < l; i++ ) {
          str->code.push_back((uint8_t) node->data->at(i));
        }
        symbol->code.push_back(opcodes::DLOOKUPSYM);
        symbol->code.push_back(str->name & 0xff);
        if( node->flags && Node::FlagEscape ) {
          symbol->code.push_back(opcodes::DPRINTE);
        } else {
          symbol->code.push_back(opcodes::DPRINT);
        }
      } else {
        std::vector<std::string>::iterator vs_it;
        int num = 0;
        for( vs_it = node->dataParts->begin(); vs_it != node->dataParts->end(); vs_it++ ) {
          // Store string in symbol
          CompilerSymbol * str = getSymbol();
          str->type = mustache::opcodes::STRING;
          for( int i = 0, l = vs_it->length(); i < l; i++ ) {
            str->code.push_back((uint8_t) vs_it->at(i));
          }
          symbol->code.push_back(num == 0 ? opcodes::DLOOKUPSYM : opcodes::DLOOKUPNRSYM);
          symbol->code.push_back(str->name & 0xff);
          num++;
        }
        if( node->flags && Node::FlagEscape ) {
          symbol->code.push_back(opcodes::DPRINTE);
        } else {
          symbol->code.push_back(opcodes::DPRINT);
        }
        int i = 0;
        for( i = 0; i < num - 1; i++ ) {
          symbol->code.push_back(opcodes::DPOP);
        }
      }
      break;
    }
  }
}

std::vector<uint8_t> * Compiler::_serialize(CompilerSymbol * main)
{
  std::vector<uint8_t> * codes = new std::vector<uint8_t>;
  std::vector<CompilerSymbol *>::iterator it;
  
  /*
  // Calculate the size of the symbol table and allocate empty space
  // First four bytes is the number of items in the symbol table
  // Each symbol has three parts:
  // One byte for the type.
  // Three bytes for the name
  // Four bytes for the location
  unsigned long header_size = 4 + symbols.size() * 8;
  codes->resize(header_size, 0);
  */
  
  // Serialize the symbols
  int * table = new int[symbols.size()];
  for( it = symbols.begin() ; it != symbols.end(); it++ ) {
    codes->push_back((*it)->type);
    codes->push_back((*it)->name);
    table[(*it)->name] = codes->size();
    codes->insert(codes->end(), (*it)->code.begin(), (*it)->code.end());
    codes->push_back(0);
  }
  
  /*
  // Write the symbol table now that we have the positions
  _PACK4A((*codes), 0, symbols.size());
  _PACK2FN(header.push_back, symbols.size());
  for( it = symbols.begin() ; it != symbols.end(); it++ ) {
    unsigned long off = (it - symbols.begin()) * 8 + 4;
    _PACK1A((*codes), off, (*it)->type);
    _PACK3A((*codes), off + 1, (*it)->name);
    _PACK4A((*codes), off + 4, table[(*it)->name]);
  }
  */
  
  // Second pass, resolve symbols to absolute jump points
  CompilerState * statec = new CompilerState(codes);
  uint8_t current = 0;
  uint8_t operand = 0;
  while( statec->next(&current, &operand) != CompilerState::END ) {
    if( statec->state != CompilerState::FUNCTION ) {
      continue;
    }
    switch( current ) {
      case opcodes::PRINTSYM:
        (*codes)[statec->spos] = opcodes::PRINTL;
        (*codes)[statec->spos+1] = table[(*codes)[statec->spos+1]];
        break;
      case opcodes::CALLSYM:
        (*codes)[statec->spos] = opcodes::CALL;
        (*codes)[statec->spos+1] = table[(*codes)[statec->spos+1]];
        break;
      case opcodes::JUMPL:
        (*codes)[statec->spos] = opcodes::JUMP;
        (*codes)[statec->spos+1] = statec->spos - statec->lpos + (*codes)[statec->spos+1]; // + 1;
        break;
      case opcodes::DLOOKUPSYM:
        (*codes)[statec->spos] = opcodes::DLOOKUP;
        (*codes)[statec->spos+1] = table[(*codes)[statec->spos+1]];
        break;
      case opcodes::DLOOKUPNRSYM:
        (*codes)[statec->spos] = opcodes::DLOOKUPNR;
        (*codes)[statec->spos+1] = table[(*codes)[statec->spos+1]];
        break;
    }
  }
  
  return codes;
}

CompilerSymbol * Compiler::getSymbol()
{
  int pos = symbols.size();
  symbols.resize(pos + 1);
  CompilerSymbol * sym = symbols[pos] = new CompilerSymbol();
  sym->name = pos;
  sym->code.reserve(32);
  return sym;
}

std::vector<uint8_t> * Compiler::compile(Node * node)
{
  symbols.clear();
  CompilerSymbol * main = _compile(node);
  return _serialize(main);
}
  
const char * Compiler::opcodeName(uint8_t code)
{
  switch( code ) {
    case opcodes::NOOP: return "NOOP"; break;
    case opcodes::SYMBOL: return "SYMBOL"; break;
    case opcodes::FUNCTION: return "FUNCTION"; break;
    case opcodes::STRING: return "STRING"; break;
    
    case opcodes::PRINTSYM: return "PRINTSYM"; break;
    case opcodes::PRINTL: return "PRINTL"; break;
    case opcodes::CALLSYM: return "CALLSYM"; break;
    case opcodes::CALL: return "CALL"; break;
    case opcodes::RETURN: return "RETURN"; break;
    case opcodes::JUMP: return "JUMP"; break;
    case opcodes::JUMPL: return "JUMPL"; break;
    case opcodes::PUSH: return "PUSH"; break;
    case opcodes::POP: return "POP"; break;
    case opcodes::INCR: return "INCR"; break;
    case opcodes::IF_GE: return "IF_GE"; break;
    
    case opcodes::DPUSH: return "DPUSH"; break;
    case opcodes::DPOP: return "DPOP"; break;
    case opcodes::DLOOKUPSYM: return "DLOOKUPSYM"; break;
    case opcodes::DLOOKUP: return "DLOOKUP"; break;
    case opcodes::DLOOKUPNRSYM: return "DLOOKUPSYM"; break;
    case opcodes::DLOOKUPNR: return "DLOOKUP"; break;
    case opcodes::DLOOKUPA: return "DLOOKUPA"; break;
    case opcodes::DARRSIZE: return "DARRSIZE"; break;
    case opcodes::DPRINT: return "DPRINT"; break;
    case opcodes::DPRINTE: return "DPRINTE"; break;
    
    case opcodes::DIF_EMPTY: return "DIF_EMPTY"; break;
    case opcodes::DIF_NOTEMPTY: return "DIF_NOTEMPTY"; break;
    case opcodes::DIF_HASH: return "DIF_HASH"; break;
    case opcodes::DIF_NOTHASH: return "DIF_NOTHASH"; break;
    case opcodes::DIF_ARRAY: return "DIF_ARRAY"; break;
    case opcodes::DIF_NOTARRAY: return "DIF_NOTARRAY"; break;
  }
  return NULL;
}

std::string * Compiler::print(uint8_t * codes, int length)
{
  std::vector<uint8_t> * vect;
  Compiler::bufferToVector(codes, length, &vect);
  std::string * ret = print(vect);
  delete vect;
  return ret;
}

std::string * Compiler::print(std::vector<uint8_t> * codes)
{
  std::string * out = new std::string;
  char buf[101];
  CompilerState * statec = new CompilerState(codes);
  uint8_t current = 0;
  uint8_t operand = 0;
  while( statec->next(&current, &operand) != CompilerState::END ) {
    switch( statec->state ) {
      case CompilerState::NOOP:
        snprintf(buf, 100, "N%03lu:      0x%02x\n", 
                statec->spos, current);
        break;
      case CompilerState::SYMBOL:
        snprintf(buf, 100, "S%03lu:      0x%02x %s %d\n", 
                statec->spos, current, opcodeName(current), operand);
        break;
      case CompilerState::FUNCTION:
        if( operand != 0 ) {
          snprintf(buf, 100, "F%03lu: %03lu: 0x%02x %s %d\n", 
                  statec->spos, statec->lpos, current, opcodeName(current), operand);
        } else {
          snprintf(buf, 100, "F%03lu: %03lu: 0x%02x %s\n", 
                  statec->spos, statec->lpos, current, opcodeName(current));
        }
        break;
      case CompilerState::STRING:
        if( current == 0x0a ) {
          snprintf(buf, 100, "C%03lu: %03lu: 0x%02x %s\n", 
                  statec->spos, statec->lpos, current, "\\n");
        } else {
          snprintf(buf, 100, "C%03lu: %03lu: 0x%02x %c\n", 
                  statec->spos, statec->lpos, current, current);
        }
        break;
      case CompilerState::HEADER:
        break;
    }
    out->append(buf);
  }
  return out;
}

void Compiler::bufferToVector(uint8_t * buffer, int length, std::vector<uint8_t> ** vector)
{
  std::vector<uint8_t> * vect = *vector = new std::vector<uint8_t>;
  vect->resize(length);
  int i = 0;
  for( ; i < length; i++, buffer++ ) {
    (*vect)[i] = *buffer;
  }
}

void Compiler::stringToBuffer(std::string * string, uint8_t ** buffer, int * length)
{
  int len = *length = string->size();
  uint8_t * buf = *buffer = (uint8_t *) calloc(sizeof(uint8_t), len);
  int i = 0;
  for( ; i < len; i++, buf++ ) {
    *buf = (uint8_t) (*string)[i];
  }
}

void Compiler::stringToVector(std::string * string, std::vector<uint8_t> ** vector)
{
  int length = string->length();
  std::vector<uint8_t> * vect = *vector = new std::vector<uint8_t>;
  vect->resize(length);
  int i = 0;
  for( ; i < length; i++ ) {
    (*vect)[i] = (uint8_t) (*string)[i];
  }
}

void Compiler::vectorToBuffer(std::vector<uint8_t> * vector, uint8_t ** buffer, int * length)
{
  int len = *length = vector->size();
  uint8_t * buf = *buffer = (uint8_t *) calloc(sizeof(uint8_t), len);
  int i = 0;
  for( ; i < len; i++, buf++ ) {
    *buf = (*vector)[i];
  }
}

}
