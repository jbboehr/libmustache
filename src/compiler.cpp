
#include <limits.h>

#include "compiler.hpp"

namespace mustache {


CompilerState::States CompilerState::next(uint8_t * code, uint8_t * operand) {
    if( this->pos > this->codes->size() ) {
      return this->state = States::END;
    } else if( this->pos == 0 ) {
      this->readHeader();
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
      str->code.push_back((uint8_t) node->data->at(i));
    }
    sym->code.push_back(opcodes::DLOOKUPSYM);
    sym->code.push_back(str->name & 0xff);
    return 1;
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
      sym->code.push_back(num == 0 ? opcodes::DLOOKUPSYM : opcodes::DLOOKUPNRSYM);
      sym->code.push_back(str->name & 0xff);
      num++;
    }
    return num;
  }
}

void Compiler::_makeLookupEnd(int num, CompilerSymbol * sym)
{
  int i = 0;
  for( ; i < num; i++ ) {
    sym->code.push_back(opcodes::DPOP);
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
            sym->code.push_back(opcodes::CALLSYM);
            sym->code.push_back(child->name);
          } else {
            _compile(*it, sym);
          }
        }
      }
      sym->code.push_back(opcodes::RETURN);
      break;
    }
    
    /* Section -------------------------------------------------------------- */
    case Node::TypeSection: {
      // Get the main symbol
      sym->type = mustache::opcodes::FUNCTION;
      
      // Don't have to do anything if empty
      if( node->children.size() <= 0 ) {
        sym->code.push_back(opcodes::RETURN);
        break;
      }
      
      // Make the section children function symbol
      CompilerSymbol * childrenSym = getSymbol();
      childrenSym->type = mustache::opcodes::FUNCTION;
      Node::Children::iterator it;
      for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
        if( (*it)->type & Node::TypeHasChildren ) {
          child = _compile(*it);
          childrenSym->code.push_back(opcodes::CALLSYM);
          childrenSym->code.push_back(child->name);
        } else {
          _compile(*it, childrenSym);
        }
      }
      childrenSym->code.push_back(opcodes::RETURN);
      
      // Make the main symbol
      
      // Push the context shifts
      int num = this->_makeLookup(node, sym);
      
      // If context empty, jump right to end
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
      
      sym->code.push_back(opcodes::INCR);
      sym->code.push_back(opcodes::JUMPL);
      sym->code.push_back(ifarrayinoppos);
      
      sym->code[ifarrayendoppos] = sym->code.size();
      sym->code.push_back(opcodes::POP);
      sym->code.push_back(opcodes::POP);
      
      sym->code[ifemptyoppos] = sym->code.size();
      sym->code[ifnotarrayoppos] = sym->code.size();
      
      // Pop the context shifts
      this->_makeLookupEnd(num, sym);
      
      sym->code.push_back(opcodes::RETURN);
      break;
    }
    
    /* Negate --------------------------------------------------------------- */
    case Node::TypeNegate: {
      sym->type = mustache::opcodes::FUNCTION;
      
      // Push the context shifts
      int num = this->_makeLookup(node, sym);
      
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
            _compile(*it, sym);
          }
        }
      }
      
      sym->code[ifemptyoppos] = sym->code.size();
      
      // Pop the context shifts
      this->_makeLookupEnd(num, sym);
      
      sym->code.push_back(opcodes::RETURN);
      break;
    }
    
    /* Output --------------------------------------------------------------- */
    case Node::TypeOutput: {
      // Store string in symbol
      CompilerSymbol * str = getSymbol();
      str->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        str->code.push_back((uint8_t) node->data->at(i));
      }
      //str->code.push_back(0x00); // The "linker" will null-terminate the string
      // Add print for symbol
      sym->code.push_back(opcodes::PRINTSYM);
      sym->code.push_back(str->name & 0xff); // @todo multi-byte
      break;
    }
    
    /* Tag ------------------------------------------------------------------ */
    case Node::TypeVariable:
    case Node::TypeTag: {
      // Push the context shifts
      int num = this->_makeLookup(node, sym);
      
      if( node->flags && Node::FlagEscape ) {
        sym->code.push_back(opcodes::DPRINTE);
      } else {
        sym->code.push_back(opcodes::DPRINT);
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
        sym->code.push_back(opcodes::CALLSYM);
        sym->code.push_back(pc_it->second);
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
        sym->code.push_back(opcodes::CALLSYM);
        sym->code.push_back(partialSym->name);
        partialSymbols.insert(std::make_pair(*(node->data), partialSym->name));
        
        // Can compile afterwards
        _compile(foundPartial, partialSym);
      } else {
        // Otherwise, maybe we should link it at runtime?
        
        // Store string in symbol
        CompilerSymbol * str = getSymbol();
        str->type = mustache::opcodes::STRING;
        for( int i = 0, l = node->data->length(); i < l; i++ ) {
          str->code.push_back((uint8_t) node->data->at(i));
        }

        sym->code.push_back(opcodes::CALLEXT);
        sym->code.push_back(str->name);
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
    codes->push_back((*it)->type);
    codes->push_back((*it)->name);
    codes->insert(codes->end(), (*it)->code.begin(), (*it)->code.end());
    codes->push_back(0);
  }
  
  // Write the symbol table now that we have the positions
  _PACK4A((*codes), 0, symbols.size());
  for( it = symbols.begin() ; it != symbols.end(); it++ ) {
    uint32_t off = (it - symbols.begin()) * 4 + 4;
    _PACK4A((*codes), off, table[(*it)->name]);
  }
  
  /*
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
   */
  
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
  compile(node, NULL);
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
  uint8_t operand = 0;
  while( statec->next(&current, &operand) != CompilerState::END ) {
    switch( statec->state ) {
      case CompilerState::NOOP:
        snprintf(buf, 100, "N%03u:      0x%02x\n", 
                statec->spos, current);
        break;
      case CompilerState::SYMBOL:
        snprintf(buf, 100, "S%03u:      0x%02x %s %d\n", 
                statec->spos, current, opcodeName(current), operand);
        break;
      case CompilerState::FUNCTION:
        if( operand != 0 ) {
          snprintf(buf, 100, "F%03u: %03u: 0x%02x %s %d\n", 
                  statec->spos, statec->lpos, current, opcodeName(current), operand);
        } else {
          snprintf(buf, 100, "F%03u: %03u: 0x%02x %s\n", 
                  statec->spos, statec->lpos, current, opcodeName(current));
        }
        break;
      case CompilerState::STRING:
        if( current == 0x0a ) {
          snprintf(buf, 100, "C%03u: %03u: 0x%02x %s\n", 
                  statec->spos, statec->lpos, current, "\\n");
        } else {
          snprintf(buf, 100, "C%03u: %03u: 0x%02x %c\n", 
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

void Compiler::bufferToVector(uint8_t * buffer, size_t length, std::vector<uint8_t> ** vector)
{
  std::vector<uint8_t> * vect = *vector = new std::vector<uint8_t>;
  vect->resize(length);
#ifdef HAVE_CXX11
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
#ifdef HAVE_CXX11
  memcpy(buf, vector->data(), len);
#else
  int i = 0;
  for( ; i < len; i++, buf++ ) {
    *buf = (*vector)[i];
  }
#endif
}

}
