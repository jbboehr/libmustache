
#include "node.hpp"

namespace mustache {


Node::~Node()
{
  // Data
  if( data != NULL ) {
    delete data;
  }
  
  // Children
  if( !childrenAreRef && children.size() > 0 ) {
    Node::Children::iterator it;
    for ( it = children.begin() ; it != children.end(); it++ ) {
      delete *it;
    }
  }
  children.clear();
}


} // namespace Mustache
