
#include "mustache.hpp"

namespace mustache {


void Mustache::tokenize(std::string * tmpl, Node * root)
{
  tokenizer.tokenize(tmpl, root);
}

void Mustache::render(std::string * tmpl, Data * data, Node::RawPartials * partials, std::string * output)
{
  Node root;
  Node::Partials partialNodes;
  
  // Tokenize
  tokenizer.tokenize(tmpl, &root);
  
  // Partials
  /*
  if( partials != NULL && partials->size() > 0 ) {
    Node::RawPartials::iterator it;
    pair<Node::Partials::iterator,bool> curPair;
    for( it = partials.begin(); it != partials.end(); it++ ) {
      // Insert node
      curPair = partialNodes.insert(Node::PartialPair(it->second));
      // Tokenize
      tokenizer.tokenize(&(curPair.first->first), &(curPair.first->second));
    }
  }
  */
  
  // Render
  renderer.init(&root, data, &partialNodes, output);
  renderer.render();
}


} // namespace Mustache
