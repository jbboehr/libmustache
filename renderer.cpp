
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
  
  // Reserver minimum length
  _output->reserve(Renderer::outputBufferLength);
  
  // Initialize stack
  if( _stack != NULL ) {
    delete _stack;
  }
  _stack = new Stack();
  _stack->push(_data);
  
  // Render
  _renderNode(_node);
  
  // Clear?
  //clear();
}

void Renderer::_renderNode(Node * node)
{
  Data * data = _stack->top();
  std::string * nstr = node->data;
  bool partialFound = false;
  
  if( data == NULL ) {
    throw Exception("Whoops, empty data");
  }
  
    // Handle simple cases
  if( node->type == Node::TypeRoot ) {
    if( node->children.size() > 0 ) {
      Node::Children::iterator it;
      for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
        _renderNode(*it);
      }
    }
    return;
  } else if( node->type == Node::TypeOutput ) {
    if( node->data != NULL && node->data->length() > 0 ) {
      _output->append(*node->data);
    }
    return;
  } else if( node->type == Node::TypeContainer ) {
    _renderNode(node->child);
    return;
  }
  
  if( nstr == NULL ) {
    throw Exception("Whoops, empty tag");
  }
  
  // Resolve data
  Data * val = NULL;
  if( data->type == Data::TypeString ) {
    // Simple
    if( nstr->compare(".") == 0 ) {
      val = data;
    }
  } else if( data->type == Data::TypeMap ) {
    // Check top level
    Data::Map::iterator it = data->data.find(*nstr);
    if( it != data->data.end() ) {
      val = it->second;
    }
  }
  
  // Data was not resolved quickly
  if( val == NULL ) {
    // Search whole stack
    
    // Dot notation
    std::string initial(*nstr);
    std::vector<std::string> parts;
    size_t found = initial.find(".");
    if( found != std::string::npos ) {
      explode(".", initial, &parts);
      if( parts.size() > 0 ) {
        initial.assign(parts.front());
      }
    }
    
    // Resolve up the data stack
    Data * ref = NULL;
    Data::Map::iterator d_it;
    int i;
    Data ** _stackPos = _stack->end();
    for( i = 0; i < _stack->size; i++, _stackPos-- ) {
      if( (*_stackPos)->type == Data::TypeMap ) {
        d_it = (*_stackPos)->data.find(initial);
        if( d_it != (*_stackPos)->data.end() ) {
          ref = d_it->second;
          if( ref != NULL ) {
            break;
          }
        }
      }
    }
    
    // Resolve or dot notation
    if( ref != NULL && parts.size() > 1 ) {
      // Dot notation
      std::vector<std::string>::iterator vs_it;
      for( vs_it = parts.begin(), vs_it++; vs_it != parts.end(); vs_it++ ) {
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
    
    if( ref != NULL ) {
      val = ref;
    }
  }
  
  // Calculate if value is empty
  bool valIsEmpty = true;
  if( val != NULL && !val->isEmpty() ) {
    valIsEmpty = false;
  }
  
  // Switch on node flags
  switch( node->flags ) {
    case Node::FlagComment:
    case Node::FlagStop:
    case Node::FlagInlinePartial:
      // Do nothing
      break;
    case Node::FlagNegate:
    case Node::FlagSection:
      // Negate/Empty list
      if( (node->flags & Node::FlagNegate) && !valIsEmpty ) {
        // Not-empty negation
        break;
      } else if( !(node->flags & Node::FlagNegate) && valIsEmpty ) {
        // Empty section
        break;
      } else if( node->children.size() <= 0 ) {
        // No children
        break;
      }
      
      if( valIsEmpty || val->type == Data::TypeString ) {
        Node::Children::iterator it;
        for( it = node->children.begin() ; it != node->children.end(); it++ ) {
          _renderNode(*it);
        }
      } else if( val->type == Data::TypeList ) {
        // Numeric array/list
        Data::List::iterator childrenIt;
        Node::Children::iterator it;
        for ( childrenIt = val->children.begin() ; childrenIt != val->children.end(); childrenIt++ ) {
          _stack->push(*childrenIt);
          for( it = node->children.begin() ; it != node->children.end(); it++ ) {
            _renderNode(*it);
          }
          _stack->pop();
        }
      } else if( val->type == Data::TypeArray ) {
        Data::Array ArrayPtr = val->array;
        int ArrayPos;
        Node::Children::iterator it;
        for ( ArrayPos = 0; ArrayPos < val->length; ArrayPos++, ArrayPtr++ ) {
          _stack->push(ArrayPtr);
          for( it = node->children.begin() ; it != node->children.end(); it++ ) {
            _renderNode(*it);
          }
          _stack->pop();
        }
      } else if( val->type == Data::TypeMap ) {
        // Associate array/map
        Node::Children::iterator it;
        _stack->push(val);
        for( it = node->children.begin() ; it != node->children.end(); it++ ) {
          _renderNode(*it);
        }
        _stack->pop();
      }
      break;
    case Node::FlagPartial:
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
    case Node::FlagEscape:
    case Node::FlagNone:
      if( !valIsEmpty && val->type == Data::TypeString ) {
        if( (bool) (node->flags & Node::FlagEscape) != true /*escapeByDefault*/ ) { // @todo escape by default
          // Probably shouldn't modify the value
          htmlspecialchars_append(val->val, _output);
        } else {
          _output->append(*val->val);
        }
      }
      break;
    default:
      //php_error("Unknown node flags");
      break;
  }
}


} // namespace Mustache
