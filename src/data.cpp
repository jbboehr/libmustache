
#include "data.hpp"

#ifdef HAVE_LIBYAML
#include "yaml.h"
#endif

#if defined(HAVE_LIBJSON)
#include "json.h"
#elif defined(HAVE_LIBJANSSON)
#include "jansson.h"
#endif

namespace mustache {


Data::~Data()
{
  switch( this->type ) {
    case Data::TypeString:
      delete val;
      break;
    case Data::TypeMap:
      if( data.size() > 0 ) {
        Data::Map::iterator dataIt;
        for ( dataIt = data.begin() ; dataIt != data.end(); dataIt++ ) {
          delete (*dataIt).second;
        }
        data.clear();
      }
      break;
    case Data::TypeList:
      if( children.size() > 0 ) {
        Data::List::iterator childrenIt;
        for ( childrenIt = children.begin() ; childrenIt != children.end(); childrenIt++ ) {
          delete *childrenIt;
        }
        children.clear();
      }
    case Data::TypeArray:
      delete[] array;
      break;
  }
}

void Data::init(Data::Type type, int size) {
  this->type = type;
  this->length = size;
  switch( type ) {
    case Data::TypeString:
      val = new std::string();
      val->reserve(size);
      break;
    case Data::TypeMap:
      // Do nothing
      break;
    case Data::TypeList:
      // Do nothing
      break;
    case Data::TypeArray:
      this->array = new Data[size];
      break;
  }
};

int Data::isEmpty()
{
  int ret = 0;
  switch( type ) {
    default:
    case Data::TypeNone:
      ret = 1;
      break;
    case Data::TypeString:
      if( val == NULL || val->length() <= 0 ) {
        ret = 1;
      }
      break;
    case Data::TypeList:
      if( children.size() <= 0 ) {
        ret = 1;
      }
      break;
    case Data::TypeMap:
      if( data.size() <= 0 ) {
        ret = 1;
      }
      break;
    case Data::TypeArray:
      if( length <= 0 ) {
        ret = 1;
      }
      break;
  }
  return ret;
}



void DataStack::push_back(Data * data)
{
  if( _size < 0 || _size >= DataStack::MAXSIZE ) {
    throw Exception("Reached max stack size");
  }
  _stack[_size] = data;
  _size++;
}

void DataStack::pop_back()
{
  if( _size > 0 ) {
    _size--;
    _stack[_size] = NULL;
  }
}

Data * DataStack::back()
{
  if( _size <= 0 ) {
    throw Exception("Reached bottom of stack");
  } else {
    return _stack[_size - 1];
  }
}

Data ** DataStack::begin()
{
  return _stack;
}

Data ** DataStack::end()
{
  return (_stack + _size - 1);
}



// Data integrations
Data & Data::createFromJSON(const char * string)
{
#if defined(HAVE_LIBJSON)
  throw Exception("JSON support using libjson not implemented");
#elif defined(HAVE_LIBJANNSON)
  throw Exception("JSON support using libjannson not implemented");
#else
  throw Exception("JSON support not enabled");
#endif
}

Data & Data::createFromYAML(const char * string)
{
#if defined(HAVE_LIBYAML)
  throw Exception("YAML support using libyaml not implemented");
#else
  throw Exception("YAML support not enabled");
#endif
}


} // namespace Mustache
