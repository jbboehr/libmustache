
#include "node.hpp"

#include "exception.hpp"
#include "utils.hpp"

namespace mustache {


Node::~Node()
{
  // Data
  if( data != NULL ) {
    delete data;
  }
  
  // Data parts
  if( dataParts != NULL ) {
    delete dataParts;
  }
  
  // Children
  if( children.size() > 0 ) {
    Node::Children::iterator it;
    for ( it = children.begin() ; it != children.end(); it++ ) {
      delete *it;
    }
  }
  children.clear();
  
  // Child should not be freed
}

void Node::setData(const std::string& data)
{
  this->data = new std::string(data);
  
  if( this->type & Node::TypeHasDot ) {
    size_t found = data.find(".");
    if( found != std::string::npos ) {
      dataParts = new std::vector<std::string>;
      explode(".", *(this->data), dataParts);
    }
  }
}



void NodeStack::push_back(Node * node)
{
  if( _size < 0 || _size >= NodeStack::MAXSIZE ) {
    throw Exception("Reached max stack size");
  }
  _stack[_size] = node;
  _size++;
}

void NodeStack::pop_back()
{
  if( _size > 0 ) {
    _size--;
    _stack[_size] = NULL;
  }
}

Node * NodeStack::back()
{
  if( _size <= 0 ) {
    throw Exception("Reached bottom of stack");
  } else {
    return _stack[_size - 1];
  }
}

Node ** NodeStack::begin()
{
  return _stack;
}

Node ** NodeStack::end()
{
  return (_stack + _size - 1);
}


} // namespace Mustache
