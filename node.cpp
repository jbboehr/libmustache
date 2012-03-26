
#include "node.hpp"

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
  
  size_t found = data.find(".");
  if( found != std::string::npos ) {
    dataParts = new std::vector<std::string>;
    explode(".", *(this->data), dataParts);
  }
}


} // namespace Mustache
