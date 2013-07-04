
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
          *code = 0;
          *operand = 0;
        }
        break;
      case States::SYMBOL:
        *code = *current;
        *operand = *next;
        this->state = this->nextState;
        if( this->state == States::FUNCTION ) {
          this->pos++;
        }
        break;
      case States::STRING:
        *code = *current;
        *operand = 0;
        if( *current == opcodes::NOOP ) {
          this->state = States::NOOP;
        }
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
        } else {
          *code = *current;
          *operand = 0;
        }
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
      sym = getSymbol();
      sym->type = mustache::opcodes::FUNCTION;
      
      CompilerSymbol * nameSym = getSymbol();
      nameSym->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        nameSym->code.push_back((uint8_t) node->data->at(i));
      }
      
      sym->code.push_back(opcodes::LOOKUPSYM);
      sym->code.push_back(nameSym->name);
      
      sym->code.push_back(opcodes::IF_EMPTY);
      sym->code.push_back(opcodes::RETURN);
      
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
    case Node::TypeNegate: {
      sym = getSymbol();
      sym->type = mustache::opcodes::FUNCTION;
      
      CompilerSymbol * nameSym = getSymbol();
      nameSym->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        nameSym->code.push_back((uint8_t) node->data->at(i));
      }
      
      sym->code.push_back(opcodes::LOOKUPSYM);
      sym->code.push_back(nameSym->name);
      
      sym->code.push_back(opcodes::IF_NOTEMPTY);
      sym->code.push_back(opcodes::RETURN);
      
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
      // Store string in symbol
      CompilerSymbol * str = getSymbol();
      str->type = mustache::opcodes::STRING;
      for( int i = 0, l = node->data->length(); i < l; i++ ) {
        str->code.push_back((uint8_t) node->data->at(i));
      }
      //str->code.push_back(0x00); // The "linker" will null-terminate the string
      // Add print for symbol
      symbol->code.push_back(opcodes::LOOKUPSYM);
      symbol->code.push_back(str->name & 0xff);
      if( node->flags && Node::FlagEscape ) {
        symbol->code.push_back(opcodes::PRINTDE);
      } else {
        symbol->code.push_back(opcodes::PRINTD);
      }
      break;
    }
  }
}

std::vector<uint8_t> * Compiler::_serialize(CompilerSymbol * main)
{
  int * table = new int[symbols.size()];
  
  std::vector<uint8_t> * codes = new std::vector<uint8_t>;
  std::vector<CompilerSymbol *>::iterator it;
  for( it = symbols.begin() ; it != symbols.end(); it++ ) {
    codes->push_back((*it)->type);
    codes->push_back((*it)->name);
    table[(*it)->name] = codes->size();
    codes->insert(codes->end(), (*it)->code.begin(), (*it)->code.end());
    codes->push_back(0);
  }
  
  // Second pass, resolve symbols to absolute jump points
  CompilerState * statec = new CompilerState(codes);
  uint8_t current = 0;
  uint8_t operand = 0;
  while( statec->next(&current, &operand) != CompilerState::END ) {
    if( statec->state != CompilerState::FUNCTION ) {
      continue;
    }
    switch( current ) {
      case opcodes::LOOKUPSYM:
        (*codes)[statec->spos] = opcodes::LOOKUP;
        (*codes)[statec->spos+1] = table[(*codes)[statec->spos+1]];
        break;
      case opcodes::PRINTSYM:
        (*codes)[statec->spos] = opcodes::PRINTL;
        (*codes)[statec->spos+1] = table[(*codes)[statec->spos+1]];
        break;
      case opcodes::CALLSYM:
        (*codes)[statec->spos] = opcodes::CALL;
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
    case opcodes::PRINTD: return "PRINTD"; break;
    case opcodes::PRINTDE: return "PRINTDE"; break;
    case opcodes::LOOKUPSYM: return "LOOKUPSYM"; break;
    case opcodes::LOOKUP: return "LOOKUP"; break;
    case opcodes::CALLSYM: return "CALLSYM"; break;
    case opcodes::CALL: return "CALL"; break;
    case opcodes::RETURN: return "RETURN"; break;
    case opcodes::IF_EMPTY: return "IF_EMPTY"; break;
    case opcodes::IF_NOTEMPTY: return "IF_NOTEMPTY"; break;
    case opcodes::IF_HASH: return "IF_HASH"; break;
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
        snprintf(buf, 100, "N%03d: 0x%02x\n", statec->spos, current);
        break;
      case CompilerState::SYMBOL:
        snprintf(buf, 100, "S%03d: 0x%02x %s %d\n", statec->spos, current, this->opcodeName(current), operand);
        break;
      case CompilerState::FUNCTION:
        if( operand != 0 ) {
          snprintf(buf, 100, "F%03d: 0x%02x %s %d\n", statec->spos, current, this->opcodeName(current), operand);
        } else {
          snprintf(buf, 100, "F%03d: 0x%02x %s\n", statec->spos, current, this->opcodeName(current));
        }
        break;
      case CompilerState::STRING:
        if( current == 0x0a ) {
          snprintf(buf, 100, "C%03d: 0x%02x %s\n", statec->spos, current, "\\n");
        } else {
          snprintf(buf, 100, "C%03d: 0x%02x %c\n", statec->spos, current, current);
        }
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
