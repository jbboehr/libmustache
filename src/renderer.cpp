
#include "renderer.hpp"

namespace mustache {


Renderer::~Renderer()
{
  clear();
}

void Renderer::clear()
{
  _node = NULL;
  _data = NULL;
  if( _stack != NULL ) {
    delete _stack;
  }
  _stack = NULL;
  _partials = NULL;
  _output = NULL;
}

void Renderer::init(Node * node, Data * data, Node::Partials * partials, std::string * output)
{
  clear();
  _node = node;
  _data = data;
  if( partials != NULL && partials->size() > 0 ) {
    // Don't add if no partials so we can check if it's null
    _partials = partials;
  }
  _output = output;
}

void Renderer::setNode(Node * node)
{
  _node = node;
}

void Renderer::setData(Data * data)
{
  _data = data;
}

void Renderer::setPartials(Node::Partials * partials)
{
  _partials = partials;
}

void Renderer::render()
{
  // Check node and data
  if( _node == NULL ) {
    throw Exception("Empty tree");
  } else if( _data == NULL ) {
    throw Exception("Empty data");
  }
  
  // Reserve minimum length (if not already set)
  if( _output->capacity() <= 0 ) {
    _output->reserve(Renderer::outputBufferLength);
  }
  
  // Initialize stack
  if( _stack != NULL ) {
    delete _stack;
  }
  _stack = new Stack<Data *>();
  _stack->push_back(_data);
  
  // Render
  _renderNode(_node);
  
  // Clear?
  //clear();
}

void Renderer::renderForLambda(Node * node, std::string * output)
{
  // Check node and data
  if( _node == NULL ) {
    throw Exception("Empty tree");
  }

  std::string * parentOutput = _output; // Swap out existing buffer
  _output = output;

  // Reserve minimum length (if not already set)
  if( _output->capacity() <= 0 ) {
    _output->reserve(Renderer::outputBufferLength);
  }

  // Render
  _renderNode(node);

  _output = parentOutput; // Put back original buffer
}

void Renderer::_renderNode(Node * node)
{
  // Check stack size?
  if( _stack->size() <= 0 ) {
    throw Exception("Whoops, empty data");
  } else if( !(node->type & Node::TypeHasNoString) && node->data == NULL ) {
    throw Exception("Whoops, empty tag");
  }
  
  // Lookup data
  bool valIsEmpty = true;
  Data * val = NULL;
  if( node->type & Node::TypeHasData ) {
    val = _lookup(node);
  }
  if( val != NULL && !val->isEmpty() ) {
    valIsEmpty = false;
  }
  
  // Switch on token type
  bool partialFound = false;
  switch( node->type ) {
    case Node::TypeComment:
    case Node::TypeStop:
    case Node::TypeInlinePartial:
      // Do nothing
      break;
      
    case Node::TypeRoot:
      if( node->children.size() > 0 ) {
        Node::Children::iterator it;
        for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
          _renderNode(*it);
        }
      }
      return;
      break;
    case Node::TypeOutput:
      if( node->data != NULL && node->data->length() > 0 ) {
        _output->append(*node->data);
      }
      return;
      break;
    case Node::TypeContainer:
      _renderNode(node->child);
      return;
      break;
      
    case Node::TypeTag:
    case Node::TypeVariable:
      if( !valIsEmpty) {
        switch( val->type ) {
          case Data::TypeString:
            if( node->flags & Node::FlagEscape ) {
              htmlspecialchars_append(val->val, _output);
            } else {
              _output->append(*val->val);
            }
            break;
          case Data::TypeLambda:
            std::string invoked = val->lambda->invoke();

            Tokenizer tokenizer;
            Node nodeFromLambda;

            tokenizer.tokenize(&invoked, &nodeFromLambda, node->flags & Node::FlagEscape);

            _renderNode(&nodeFromLambda);
            break;
        }
      }
      break;
      
    case Node::TypeNegate:
      if( valIsEmpty ) {
        Node::Children::iterator it;
        for( it = node->children.begin() ; it != node->children.end(); it++ ) {
          _renderNode(*it);
        }
      }
      break;
      
    case Node::TypeSection:
      if( !valIsEmpty ) {
        switch( val->type ) {
          default:
          case Data::TypeString:
            for( Node::Children::iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
              _renderNode(*it);
            }
            break;
          case Data::TypeList:
            for( Data::List::iterator childrenIt = val->children.begin() ; childrenIt != val->children.end(); childrenIt++ ) {
              _stack->push_back(*childrenIt);
              for( Node::Children::iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
                _renderNode(*it);
              }
              _stack->pop_back();
            }
            break;
          case Data::TypeArray:
        	  for( int i = 0; i < val->length; i++ ) {
                  _stack->push_back(val->array[i]);
                  for( Node::Children::iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
                    _renderNode(*it);
                  }
                  _stack->pop_back();
        	  }
            break;
          case Data::TypeMap:
            // Associate array/map
            _stack->push_back(val);
            for( Node::Children::iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
              _renderNode(*it);
            }
            _stack->pop_back();
            break;
          case Data::TypeLambda:
            std::string text = node->children_to_template_string(*node->startSequence, *node->stopSequence);
            std::string invoked = val->lambda->invoke(&text, this);

            Tokenizer tokenizer;
            Node nodeFromLambda;

            tokenizer.setStartSequence(*node->startSequence);
            tokenizer.setStopSequence(*node->stopSequence);
            tokenizer.tokenize(&invoked, &nodeFromLambda, node->flags & Node::FlagEscape);

            _renderNode(&nodeFromLambda);
            break;
        }
      }
      break;
      
    case Node::TypePartial:
      if( !partialFound && _partials != NULL ) {
        Node::Partials::iterator p_it;
        p_it = _partials->find(*(node->data));
        if( p_it != _partials->end() ) {
          partialFound = true;
          _renderNode(&(p_it->second));
        }
      }
      if( !partialFound && _node->partials.size() > 0 ) {
        Node::Partials::iterator p_it;
        p_it = _node->partials.find(*(node->data));
        if( p_it != _node->partials.end() ) {
          partialFound = true;
          _renderNode(&(p_it->second));
        }
      }
      break;
      
    default:
      //php_error("Unknown node flags");
      break;
  }
}

Data * Renderer::_lookup(Node * node)
{
  Data * data = _stack->back();
  
  if( data->type == Data::TypeString ) {
    // Simple
    if( node->data->compare(".") == 0 ) {
      return data;
    }
  } else if( data->type == Data::TypeMap ) {
    // Check top level
    Data::Map::iterator it = data->data.find(*(node->data));
    if( it != data->data.end() ) {
      return it->second;
    }
  } 
  
  // Stop here for strict paths
  if( this->_strictPaths ) {
    return NULL;
  }
  
  // Get initial segment for dot notation
  std::string * initial;
  if( node->dataParts != NULL ) {
    initial = &(node->dataParts->at(0));
  } else {
    initial = node->data;
  }
  
  // Resolve up the data stack
  Data * ref = NULL;
  Data::Map::iterator d_it;
  register int i;
  Data ** _stackPos = _stack->end();
  for( i = 0; i < _stack->size(); i++, _stackPos-- ) {
    if( (*_stackPos)->type == Data::TypeMap ) {
      d_it = (*_stackPos)->data.find(*initial);
      if( d_it != (*_stackPos)->data.end() ) {
        ref = d_it->second;
        if( ref != NULL ) {
          break;
        }
      }
    }
  }

  // Resolve or dot notation
  if( ref != NULL && node->dataParts != NULL && node->dataParts->size() > 1 ) {
    // Dot notation
    std::vector<std::string>::iterator vs_it;
    for( vs_it = node->dataParts->begin(), vs_it++; vs_it != node->dataParts->end(); vs_it++ ) {
      if( ref == NULL ) {
        break;
      } else if( ref->type != Data::TypeMap ) {
        ref = NULL; // Not sure about this
        break;
      } else {
        d_it = ref->data.find(*vs_it);
        if( d_it == ref->data.end() ) {
          ref = NULL; // Not sure about this
          break; 
        }
        ref = d_it->second;
      }
    }
  }
  
  return ref;
}


} // namespace Mustache
