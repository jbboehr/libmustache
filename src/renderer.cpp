
#include "renderer.hpp"

#include <utility>

namespace mustache {


Renderer::~Renderer()
{
  clear();
}

void Renderer::clear()
{
  _node = NULL;
  _data = NULL;
  _stack.clear();
  _partials = NULL;
  _partialResolver = PartialResolver();
  _output = NULL;
}

void Renderer::init(const Node * node, const Data * data,
    const Node::Partials * partials, std::string * output)
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

void Renderer::setNode(const Node * node)
{
  _node = node;
}

void Renderer::setData(const Data * data)
{
  _data = data;
}

void Renderer::setPartials(const Node::Partials * partials)
{
  _partials = partials;
}

void Renderer::setPartialResolver(PartialResolver resolver)
{
  _partialResolver = std::move(resolver);
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
  _stack.clear();
  _stack.push_back(_data);
  
  // Render
  _renderNode(_node);
  
  // Clear?
  //clear();
}

void Renderer::renderForLambda(const Node * node, std::string * output)
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

void Renderer::_renderNode(const Node * node)
{
  if( node == NULL ) {
    throw Exception("Empty tree node");
  }

  // Check stack size?
  if( _stack.size() <= 0 ) {
    throw Exception("Whoops, empty data");
  } else if( !(node->type & Node::TypeHasNoString) &&
      !node->data.has_value() ) {
    throw Exception("Whoops, empty tag");
  }
  
  // Lookup data
  bool valIsEmpty = true;
  const Data * val = NULL;
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
        Node::Children::const_iterator it;
        for ( it = node->children.begin() ; it != node->children.end(); it++ ) {
          _renderNode(it->get());
        }
      }
      return;
      break;
    case Node::TypeOutput:
      if( node->data.has_value() && node->data->length() > 0 ) {
        _output->append(*node->data);
      }
      return;
      break;
    case Node::TypeContainer:
      if( node->child == NULL ) {
        throw Exception("Empty container node");
      }
      _renderNode(node->child.get());
      return;
      break;
      
    case Node::TypeTag:
    case Node::TypeVariable:
      if( !valIsEmpty) {
        switch( val->type() ) {
          case Data::TypeString:
          case Data::TypeBoolean:
          case Data::TypeInteger:
          case Data::TypeDouble: {
            const std::string rendered = val->toString();
            if( node->flags & Node::FlagEscape ) {
              htmlspecialchars_append(rendered, _output);
            } else {
              _output->append(rendered);
            }
            break;
          }
          case Data::TypeLambda: {
            std::string invoked = val->lambdaValue()->invoke();

            Tokenizer tokenizer;
            Node nodeFromLambda;

            tokenizer.tokenize(&invoked, &nodeFromLambda, node->flags & Node::FlagEscape);

            _renderNode(&nodeFromLambda);
            break;
          }
          case Data::TypeNone:
          case Data::TypeList:
          case Data::TypeMap:
          case Data::TypeArray:
            break;
        }
      }
      break;
      
    case Node::TypeNegate:
      if( valIsEmpty ) {
        Node::Children::const_iterator it;
        for( it = node->children.begin() ; it != node->children.end(); it++ ) {
          _renderNode(it->get());
        }
      }
      break;
      
    case Node::TypeSection:
      if( !valIsEmpty ) {
        switch( val->type() ) {
          default:
          case Data::TypeString:
          case Data::TypeBoolean:
          case Data::TypeInteger:
          case Data::TypeDouble:
            _stack.push_back(val);
            for( Node::Children::const_iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
              _renderNode(it->get());
            }
            _stack.pop_back();
            break;
          case Data::TypeList:
            for( const Data& child : val->listItems() ) {
              _stack.push_back(&child);
              for( Node::Children::const_iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
                _renderNode(it->get());
              }
              _stack.pop_back();
            }
            break;
          case Data::TypeArray:
            for( const Data& child : val->arrayItems() ) {
              _stack.push_back(&child);
              for( Node::Children::const_iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
                _renderNode(it->get());
              }
              _stack.pop_back();
            }
            break;
          case Data::TypeMap:
            // Associate array/map
            _stack.push_back(val);
            for( Node::Children::const_iterator it = node->children.begin() ; it != node->children.end(); it++ ) {
              _renderNode(it->get());
            }
            _stack.pop_back();
            break;
          case Data::TypeLambda: {
            if( !node->startSequence.has_value() ||
                !node->stopSequence.has_value() ) {
              throw Exception("Missing section delimiters");
            }
            std::string text = node->children_to_template_string(*node->startSequence, *node->stopSequence);
            std::string invoked = val->lambdaValue()->invoke(
                std::string_view(text), this);

            Tokenizer tokenizer;
            Node nodeFromLambda;

            tokenizer.setStartSequence(*node->startSequence);
            tokenizer.setStopSequence(*node->stopSequence);
            tokenizer.tokenize(&invoked, &nodeFromLambda, node->flags & Node::FlagEscape);

            _renderNode(&nodeFromLambda);
            break;
          }
          case Data::TypeNone:
            break;
        }
      }
      break;
      
    case Node::TypePartial:
      if( !partialFound && _partialResolver ) {
        const Node * partial = _partialResolver(*node->data);
        if( partial != NULL ) {
          partialFound = true;
          _renderNode(partial);
        }
      }
      if( !partialFound && _partials != NULL ) {
        Node::Partials::const_iterator p_it;
        p_it = _partials->find(*(node->data));
        if( p_it != _partials->end() && p_it->second != NULL ) {
          partialFound = true;
          _renderNode(p_it->second.get());
        }
      }
      if( !partialFound && _node->partials.size() > 0 ) {
        Node::Partials::const_iterator p_it;
        p_it = _node->partials.find(*(node->data));
        if( p_it != _node->partials.end() && p_it->second != NULL ) {
          partialFound = true;
          _renderNode(p_it->second.get());
        }
      }
      break;
      
    default:
      //php_error("Unknown node flags");
      break;
  }
}

const Data * Renderer::_lookup(const Node * node)
{
  const Data * data = _stack.back();
  
  if( data->type() != Data::TypeMap &&
      data->type() != Data::TypeList && data->type() != Data::TypeArray ) {
    // Simple
    if( node->data->compare(".") == 0 ) {
      return data;
    }
  } else if( data->type() == Data::TypeMap ) {
    // Check top level
    const Data * found = data->find(*node->data);
    if( found != NULL ) {
      return found;
    }
  } 
  
  // Stop here for strict paths
  if( this->_strictPaths ) {
    return NULL;
  }
  
  // Get initial segment for dot notation
  const std::string * initial;
  if( !node->dataParts.empty() ) {
    initial = &(node->dataParts.at(0));
  } else {
    initial = &*node->data;
  }
  
  // Resolve up the data stack
  const Data * ref = NULL;
  int i;
  const Data ** stackPos = _stack.end();
  for( i = 0; i < _stack.size(); i++, stackPos-- ) {
    if( *stackPos != NULL && (*stackPos)->type() == Data::TypeMap ) {
      ref = (*stackPos)->find(*initial);
      if( ref != NULL ) {
        break;
      }
    }
  }

  // Resolve or dot notation
  if( ref != NULL && node->dataParts.size() > 1 ) {
    // Dot notation
    std::vector<std::string>::const_iterator vs_it;
    for( vs_it = node->dataParts.begin(), vs_it++;
        vs_it != node->dataParts.end(); vs_it++ ) {
      if( ref == NULL ) {
        break;
      } else if( ref->type() != Data::TypeMap ) {
        ref = NULL; // Not sure about this
        break;
      } else {
        ref = ref->find(*vs_it);
        if( ref == NULL ) {
          ref = NULL; // Not sure about this
          break; 
        }
      }
    }
  }
  
  return ref;
}


} // namespace Mustache
