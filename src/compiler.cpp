
#include <limits.h>

#include "compiler.hpp"

namespace mustache {


int CompilerState::next(uint8_t * code, _C_OP_TYPE * operand) {
    if( this->pos >= this->codes->size() ) {
      return this->state = CompilerStates::END;
    } else if( this->pos == 0 ) {
      this->readHeader();
    }
    this->spos = this->pos;
    uint8_t * current = &((*this->codes)[this->pos]);
    //uint8_t * next = &((*this->codes)[this->pos + 1]);
    _C_OP_TYPE next = _C_OP_UNPACKA((*this->codes), this->pos + 1);
    switch( this->state ) {
      case CompilerStates::NOOP:
        if( *current == opcodes::FUNCTION ) {
          this->state = CompilerStates::SYMBOL;
          this->nextState = CompilerStates::FUNCTION;
          *code = *current;
          *operand = next;
          this->pos += _C_OP_SIZE; // Skip operand
        } else if( *current == opcodes::STRING ) {
          this->state = CompilerStates::SYMBOL;
          this->nextState = CompilerStates::STRING;
          *code = *current;
          *operand = next;
          this->pos += _C_OP_SIZE; // Skip operand
        } else {
          this->state = CompilerStates::NOOP;
          *code = *current;
          *operand = 0;
        }
        this->lpos = 0;
        break;
      case CompilerStates::SYMBOL:
        *code = *current;
        *operand = next;
        this->state = this->nextState;
        if( this->state == CompilerStates::FUNCTION ) {
          this->pos += _C_OP_SIZE;
        }
        this->lpos = 0;
        break;
      case CompilerStates::STRING:
        *code = *current;
        *operand = 0;
        if( *current == opcodes::NOOP ) {
          this->state = CompilerStates::NOOP;
        }
        this->lpos += 1;
        break;
      case CompilerStates::FUNCTION:
        if( *current == opcodes::NOOP ) {
          this->state = CompilerStates::NOOP;
          *code = *current;
          *operand = 0;
        } else if( true == Compiler::hasOperand(*current) ) {
          *code = *current;
          *operand = next;
          this->pos += _C_OP_SIZE; // Skip operand
          this->lpos += _C_OP_SIZE;
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

void CompilerState::readHeader()
{
  // Read the symbol table
  this->numSymbols = _UNPACK4A((*this->codes), 0);
  int i = 0;
  for( ; i < this->numSymbols; i++ ) {
    this->symbols.push_back(_UNPACK4A((*this->codes), 4 + 4 * i));
  }
  this->pos = 4 + 4 * this->numSymbols;
}

int Compiler::_makeLookup(Node * node, CompilerSymbol * sym)
{
  // Do nothing for the implicit iterator
  if( node->data->compare(".") == 0 ) {
    return 0;
  }
  
  if( node->dataParts == NULL ) {
    // Store string in symbol
    CompilerSymbol * str = getSymbol();
    str->type = mustache::opcodes::STRING;
    for( int i = 0, l = node->data->length(); i < l; i++ ) {
      _CPUSH(str->code, node->data->at(i));
    }
    _CPUSHOP(sym->code, opcodes::DLOOKUPSYM, str->name);
    return 1;
  } else {
    std::vector<std::string>::iterator vs_it;
    int num = 0;
    for( vs_it = node->dataParts->begin(); vs_it != node->dataParts->end(); vs_it++ ) {
      // Store string in symbol
      CompilerSymbol * str = getSymbol();
      str->type = mustache::opcodes::STRING;
      for( int i = 0, l = vs_it->length(); i < l; i++ ) {
        _CPUSH(str->code, vs_it->at(i));
      }
      _CPUSHOP(sym->code, 
               num == 0 ? opcodes::DLOOKUPSYM : opcodes::DLOOKUPNRSYM,
               str->name);
      num++;
    }
    return num;
  }
}

void Compiler::_makeLookupEnd(int num, CompilerSymbol * sym)
{
  int i = 0;
  for( ; i < num; i++ ) {
    _CPUSH(sym->code, opcodes::DPOP);
  }
}

CompilerSymbol * Compiler::_compile(Node * node)
{
  CompilerSymbol * sym = getSymbol();
  _compile(node, sym);
  return sym;
}

void Compiler::_compile(Node * node, CompilerSymbol * sym)
{
  CompilerSymbol * child = NULL;
  
  switch( node->type ) {
    
    /* Root ----------------------------------------------------------------- */
    case Node::TypeRoot: {
      sym->type = mustache::opcodes::FUNCTION;
      if( node->children.size() > 0 ) {
        Node::Children::iterator it;
        for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
          if( (*it)->type & Node::TypeHasChildren ) {
            child = _compile(*it);
            _CPUSHOP(sym->code, opcodes::CALLSYM, child->name);
          } else {
            _compile(*it, sym);
          }
        }
      }
      _CPUSH(sym->code, opcodes::RETURN);
      break;
    }
    
    /* Section -------------------------------------------------------------- */
    case Node::TypeSection: {
      // Get the main symbol
      sym->type = mustache::opcodes::FUNCTION;
      
      // Don't have to do anything if empty
      if( node->children.size() <= 0 ) {
        _CPUSH(sym->code, opcodes::RETURN);
        break;
      }
      
      // Make the section children function symbol
      CompilerSymbol * childrenSym = getSymbol();
      childrenSym->type = mustache::opcodes::FUNCTION;
      Node::Children::iterator it;
      for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
        if( (*it)->type & Node::TypeHasChildren ) {
          child = _compile(*it);
          _CPUSHOP(childrenSym->code, opcodes::CALLSYM, child->name);
        } else {
          _compile(*it, childrenSym);
        }
      }
      _CPUSH(childrenSym->code, opcodes::RETURN);
      
      // Make the main symbol
      
      // Push the context shifts
      int num = this->_makeLookup(node, sym);
      
      // If context empty, jump right to end
      _CPUSH(sym->code, opcodes::DIF_EMPTY);
      _CPUSHOP(sym->code, opcodes::JUMPL, 0x00);
      size_t ifemptyoppos = _CLENP(sym->code);
      
      // If array, jump to the array loop
      _CPUSH(sym->code, opcodes::DIF_ARRAY);
      _CPUSHOP(sym->code, opcodes::JUMPL, 0x00);
      size_t ifarrayoppos = _CLENP(sym->code);
      
      // If not array, just call then jump to end
      _CPUSHOP(sym->code, opcodes::CALLSYM, childrenSym->name);
      _CPUSHOP(sym->code, opcodes::JUMPL, 0x00);
      size_t ifnotarrayoppos = _CLENP(sym->code);
      
      // Do the array looping code
      _CSETOP(sym->code, ifarrayoppos, _CLEN(sym->code));
      _CPUSH(sym->code, opcodes::DARRSIZE);
      _CPUSHOP(sym->code, opcodes::PUSH, 0x00);
      size_t ifarrayinoppos = _CLEN(sym->code); // not a typo
      
      _CPUSH(sym->code, opcodes::IF_GE);
      _CPUSHOP(sym->code, opcodes::JUMPL, 0x00);
      size_t ifarrayendoppos = _CLENP(sym->code);
      
      _CPUSH(sym->code, opcodes::DLOOKUPA);
      
      _CPUSHOP(sym->code, opcodes::CALLSYM, childrenSym->name);
      
      _CPUSH(sym->code, opcodes::DPOP);
      
      _CPUSH(sym->code, opcodes::INCR);
      _CPUSHOP(sym->code, opcodes::JUMPL, ifarrayinoppos);
      
      _CSETOP(sym->code, ifarrayendoppos, _CLEN(sym->code));
      _CPUSH(sym->code, opcodes::POP);
      _CPUSH(sym->code, opcodes::POP);
      
      _CSETOP(sym->code, ifemptyoppos, _CLEN(sym->code));
      _CSETOP(sym->code, ifnotarrayoppos, _CLEN(sym->code));
      
      // Pop the context shifts
      this->_makeLookupEnd(num, sym);
      
      _CPUSH(sym->code, opcodes::RETURN);
      break;
    }
    
    /* Negate --------------------------------------------------------------- */
    case Node::TypeNegate: {
      sym->type = mustache::opcodes::FUNCTION;
      
      // Push the context shifts
      int num = this->_makeLookup(node, sym);
      
      _CPUSH(sym->code, opcodes::DIF_NOTEMPTY);
      _CPUSHOP(sym->code, opcodes::JUMPL, 0x00);
      size_t ifemptyoppos = _CLENP(sym->code);
      
      if( node->children.size() > 0 ) {
        Node::Children::iterator it;
        for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
          if( (*it)->type & Node::TypeHasChildren ) {
            child = _compile(*it);
            _CPUSHOP(sym->code, opcodes::CALLSYM, child->name);
          } else {
            _compile(*it, sym);
          }
        }
      }
      
      _CSETOP(sym->code, ifemptyoppos, _CLEN(sym->code));
      
      // Pop the context shifts
      this->_makeLookupEnd(num, sym);
      
      _CPUSH(sym->code, opcodes::RETURN);
      break;
    }
    
    /* Output --------------------------------------------------------------- */
    case Node::TypeOutput: {
      // Store string in symbol
      CompilerSymbol * str = getSymbol();
      str->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        _CPUSH(str->code, node->data->at(i));
      }
      // Add print for symbol
      _CPUSHOP(sym->code, opcodes::PRINTSYM, str->name);
      break;
    }
    
    /* Tag ------------------------------------------------------------------ */
    case Node::TypeVariable:
    case Node::TypeTag: {
      // Push the context shifts
      int num = this->_makeLookup(node, sym);
      
      if( node->flags && Node::FlagEscape ) {
        _CPUSH(sym->code, opcodes::DPRINTE);
      } else {
        _CPUSH(sym->code, opcodes::DPRINT);
      }
      
      // Pop the context shifts
      this->_makeLookupEnd(num, sym);
      break;
    }
    
    /* Partial -------------------------------------------------------------- */
    case Node::TypePartial: {
      // Check for a partial that's already been compiled
      std::map<std::string,int>::iterator pc_it = partialSymbols.find(*(node->data));
      if( pc_it != partialSymbols.end() ) {
        _CPUSHOP(sym->code, opcodes::CALLSYM, pc_it->second);
        break;
      }
      
      // See if we have a partial
      Node * foundPartial = NULL;
      if( partials != NULL ) {
        Node::Partials::iterator p_it;
        p_it = partials->find(*(node->data));
        if( p_it != partials->end() ) {
          foundPartial = &(p_it->second);
        }
      }
      
      // If we have a partial, compile it in and call it, and add it to the 
      // map of compiled partials
      if( foundPartial != NULL ) {
        // Need to get and store the symbol now to support recursion
        CompilerSymbol * partialSym = getSymbol();
        _CPUSHOP(sym->code, opcodes::CALLSYM, partialSym->name);
        partialSymbols.insert(std::make_pair(*(node->data), partialSym->name));
        
        // Can compile afterwards
        _compile(foundPartial, partialSym);
      } else {
        // Otherwise, maybe we should link it at runtime?
        
        // Store string in symbol
        CompilerSymbol * str = getSymbol();
        str->type = mustache::opcodes::STRING;
        for( int i = 0, l = node->data->length(); i < l; i++ ) {
          _CPUSH(str->code, node->data->at(i));
        }
        
        _CPUSHOP(sym->code, opcodes::CALLEXT, str->name);
      }
    }
    
    /* Other ---------------------------------------------------------------- */
    case Node::TypeComment:
      // Ignore comments
      break;
    case Node::TypeStop:
      // Ignore close tags (already parsed)
      break;
    default: {
      // Throw exception on unknown token type
      std::ostringstream oss;
      oss << "Unknown type" << node->type;
      throw Exception(oss.str());
    }
  }
}

std::vector<uint8_t> * Compiler::_serialize(CompilerSymbol * main)
{
  std::vector<uint8_t> * codes = new std::vector<uint8_t>;
  std::vector<CompilerSymbol *>::iterator it;
  
  // Calculate the size of the symbol table and allocate empty space
  // First four bytes is the number of items in the symbol table
  // Each symbol is four bytes for the location
  uint32_t header_size = 4 + symbols.size() * 4;
  codes->resize(header_size, 0);
  
  // Serialize the symbols
  uint32_t * table = new uint32_t[symbols.size()];
  for( it = symbols.begin() ; it != symbols.end(); it++ ) {
    table[(*it)->name] = codes->size();
    _CPUSHOP((*codes), (*it)->type, (*it)->name);
//    codes->push_back((*it)->type);
//    codes->push_back((*it)->name);
    codes->insert(codes->end(), (*it)->code.begin(), (*it)->code.end());
    codes->push_back(0);
  }
  
  // Write the symbol table now that we have the positions
  _PACK4A((*codes), 0, symbols.size());
  for( it = symbols.begin() ; it != symbols.end(); it++ ) {
    uint32_t off = (it - symbols.begin()) * 4 + 4;
    _PACK4A((*codes), off, table[(*it)->name]);
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
  return compile(node, NULL);
}

std::vector<uint8_t> * Compiler::compile(Node * node, Node::Partials * partials)
{
  this->symbols.clear();
  this->partials = partials;
  this->partialSymbols.clear();
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
    
    case opcodes::CALLEXT: return "CALLEXT"; break;
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
  buf[0] = '\0';
  CompilerState * statec = new CompilerState(codes);
  
  // Print symbol table
  uint32_t i = 0;
  for( ; i < statec->symbols.size(); i++ ) {
    snprintf(buf, 100, "Symbol %03u @ %03u\n", i, statec->symbols[i]);
    out->append(buf);
  }
  
  
  // Print symbols
  uint8_t current = 0;
  _C_OP_TYPE operand = 0;
  while( statec->next(&current, &operand) != CompilerStates::END ) {
    switch( statec->state ) {
      case CompilerStates::NOOP:
        snprintf(buf, 100, "N%03u:      0x%02x\n", 
                statec->spos, current);
        break;
      case CompilerStates::SYMBOL:
        snprintf(buf, 100, "S%03u:      0x%02x %s %d\n", 
                statec->spos, current, opcodeName(current), operand);
        break;
      case CompilerStates::FUNCTION:
        if( operand != 0 ) {
          snprintf(buf, 100, "F%03u: %03u: 0x%02x %s %d\n", 
                  statec->spos, statec->lpos, current, opcodeName(current), operand);
        } else {
          snprintf(buf, 100, "F%03u: %03u: 0x%02x %s\n", 
                  statec->spos, statec->lpos, current, opcodeName(current));
        }
        break;
      case CompilerStates::STRING:
        if( current == 0x0a ) {
          snprintf(buf, 100, "C%03u: %03u: 0x%02x %s\n", 
                  statec->spos, statec->lpos, current, "\\n");
        } else {
          snprintf(buf, 100, "C%03u: %03u: 0x%02x %c\n", 
                  statec->spos, statec->lpos, current, current);
        }
        break;
      case CompilerStates::HEADER:
        break;
    }
    out->append(buf);
  }
  return out;
}

void Compiler::bufferToVector(uint8_t * buffer, size_t length, std::vector<uint8_t> ** vector)
{
  std::vector<uint8_t> * vect = *vector = new std::vector<uint8_t>;
  vect->resize(length);
#ifdef MUSTACHE_HAVE_CXX11
  memcpy(vect->data(), buffer, length);
#else
  int i = 0;
  for( ; i < length; i++, buffer++ ) {
    (*vect)[i] = *buffer;
  }
#endif
}

void Compiler::stringToBuffer(std::string * string, uint8_t ** buffer, size_t * length)
{
  size_t len = *length = string->size();
  uint8_t * buf = *buffer = (uint8_t *) malloc(sizeof(uint8_t) * len);
#if CHAR_BIT == 8
  memcpy(buf, string->data(), len);
#else
  int i = 0;
  for( ; i < len; i++, buf++ ) {
    *buf = (uint8_t) (*string)[i];
  }
#endif
}

void Compiler::stringToVector(std::string * string, std::vector<uint8_t> ** vector)
{
  size_t length = string->length();
  std::vector<uint8_t> * vect = *vector = new std::vector<uint8_t>;
  vect->resize(length);
#if CHAR_BIT == 8 && defined(HAVE_CXX11)
  memcpy(vect->data(), string->data(), length);
#else
  int i = 0;
  for( ; i < length; i++ ) {
    (*vect)[i] = (uint8_t) (*string)[i];
  }
#endif
}

void Compiler::vectorToBuffer(std::vector<uint8_t> * vector, uint8_t ** buffer, size_t * length)
{
  size_t len = *length = vector->size();
  uint8_t * buf = *buffer = (uint8_t *) malloc(sizeof(uint8_t) * len);
#ifdef MUSTACHE_HAVE_CXX11
  memcpy(buf, vector->data(), len);
#else
  int i = 0;
  for( ; i < len; i++, buf++ ) {
    *buf = (*vector)[i];
  }
#endif
}

}
