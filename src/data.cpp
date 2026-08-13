
#include "data.hpp"

#ifdef MUSTACHE_HAVE_LIBYAML
#include "yaml.h"
#endif

#if defined(MUSTACHE_HAVE_LIBJSON)
#include "json.h"
#include "json_object.h"
#include "json_tokener.h"
#endif

#include "stdio.h"

#include <utility>

namespace mustache {


Data::Data(Data&& other) noexcept :
    type(Data::TypeNone),
    length(0),
    val(NULL),
    lambda(NULL)
{
  swap(other);
}

Data& Data::operator=(Data&& other) noexcept
{
  if( this != &other ) {
    Data previous(std::move(other));
    swap(previous);
  }
  return *this;
}

void Data::swap(Data& other) noexcept
{
  using std::swap;
  swap(type, other.type);
  swap(length, other.length);
  swap(val, other.val);
  data.swap(other.data);
  children.swap(other.children);
  array.swap(other.array);
  swap(lambda, other.lambda);
}


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
      if( array.size() > 0 ) {
        Data::Array::iterator arrayIt;
        for ( arrayIt = array.begin() ; arrayIt != array.end(); arrayIt++ ) {
          delete *arrayIt;
        }
        array.clear();
      }
      break;
    case Data::TypeLambda:
      delete lambda;
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
      this->array.reserve(size);
      break;
    case Data::TypeLambda:
      // Do nothing
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
    case Data::TypeLambda:
      if( lambda == NULL ) {
        ret = 1;
      }
      break;
  }
  return ret;
}





Data * searchStack(Stack<Data *> * stack, std::string * key)
{
  // Resolve up the data stack
  Data * ref = NULL;
  Data::Map::iterator d_it;
  int i;
  Data ** _stackPos = stack->end();
  for( i = 0; i < stack->size(); i++, _stackPos-- ) {
    if( (*_stackPos) == NULL ) continue;
    if( (*_stackPos)->type == Data::TypeMap ) {
      d_it = (*_stackPos)->data.find(*key);
      if( d_it != (*_stackPos)->data.end() ) {
        ref = d_it->second;
        if( ref != NULL ) {
          break;
        }
      }
    }
  }
  return ref;
}

Data * searchStackNR(Stack<Data *> * stack, std::string * key)
{
  Data * ref = NULL;
  Data * back = stack->back();
  Data::Map::iterator d_it;
  if( back != NULL && back->type == Data::TypeMap ) {
    d_it = back->data.find(*key);
    if( d_it != back->data.end() ) {
      ref = d_it->second;
      if( ref != NULL ) {
        return ref;
      }
    }
  }
  return NULL;
}



// Data integrations

#if defined(MUSTACHE_HAVE_LIBJSON)
static void _createFromJSON(Data * data, struct json_object * object)
{
  switch( json_object_get_type(object) ) {
    case json_type_null:
      data->type = Data::TypeString;
      data->val = new std::string("");
      break;
    case json_type_boolean:
      data->type = Data::TypeString;
      if( 0 == (int) json_object_get_boolean(object) ) {
        data->val = new std::string("");
      } else {
        data->val = new std::string("true");
      }
      break;
    case json_type_double:
    case json_type_int:
    case json_type_string:
      data->type = Data::TypeString;
      data->val = new std::string(json_object_get_string(object));
      break;
    case json_type_object: {
      data->type = Data::TypeMap;
      std::string ckey;
      json_object_object_foreach(object, key, value)
      {
        ckey.assign(key);
        std::unique_ptr<Data> ownedChild(new Data());
        _createFromJSON(ownedChild.get(), value);
        if( data->data.insert(std::make_pair(ckey, ownedChild.get())).second ) {
          ownedChild.release();
        }
      }
      break;
    }
    case json_type_array: {
      int len = json_object_array_length(object);
      data->init(Data::TypeArray, len);
      
      struct json_object * array_item;
      for( int i = 0; i < len; i++ ) {
        array_item = json_object_array_get_idx(object, i);
        std::unique_ptr<Data> ownedChild(new Data());
        _createFromJSON(ownedChild.get(), array_item);
        data->array.push_back(ownedChild.get());
        ownedChild.release();
      }
      data->length = static_cast<int>(data->array.size());
      break;
    }
    default: {
      throw Exception("Unknown json type");
    }
  }
}

Data * Data::createFromJSON(const char * string)
{
  struct json_object * result = json_tokener_parse((char *) string);
  if( result == NULL ) {
    throw Exception("Invalid JSON data");
  }
  std::unique_ptr<Data> data(new Data());
  try {
    _createFromJSON(data.get(), result);
  } catch( ... ) {
    json_object_put(result);
    throw;
  }
  json_object_put(result);
  return data.release();
}
#else
Data * Data::createFromJSON(const char * string)
{
  throw Exception("JSON support not enabled");
}
#endif

#if defined(MUSTACHE_HAVE_LIBYAML)
static void _createFromYAML(Data * data, yaml_document_t * document, yaml_node_t * node)
{
  switch( node->type ) {
    case YAML_SCALAR_NODE: {
      char * value = reinterpret_cast<char *>(node->data.scalar.value);
      data->type = Data::TypeString;
      data->val = new std::string(value);
      break;
    }
    case YAML_MAPPING_NODE: {
      data->type = Data::TypeMap;
      std::string ckey;
      yaml_node_pair_t * pair;
      for( pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++ ) {
        yaml_node_t * keyNode = yaml_document_get_node(document, pair->key);
        yaml_node_t * valueNode = yaml_document_get_node(document, pair->value);
        char * keyValue = reinterpret_cast<char *>(keyNode->data.scalar.value);
        
        ckey.assign(keyValue);
        std::unique_ptr<Data> ownedChild(new Data());
        _createFromYAML(ownedChild.get(), document, valueNode);
        if( data->data.insert(std::make_pair(ckey, ownedChild.get())).second ) {
          ownedChild.release();
        }
      }
      break;
    }
    case YAML_SEQUENCE_NODE: {
      int len = (node->data.sequence.items.top - node->data.sequence.items.start);
      data->init(Data::TypeArray, len);
      
      yaml_node_item_t * item;
      for( item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++ ) {
        yaml_node_t * valueNode = yaml_document_get_node(document, *item);
        std::unique_ptr<Data> ownedChild(new Data());
        _createFromYAML(ownedChild.get(), document, valueNode);
        data->array.push_back(ownedChild.get());
        ownedChild.release();
      }
      data->length = static_cast<int>(data->array.size());
      break;
    }
    default: {
      throw Exception("Unknown yaml type");
    }
  }
}

Data * Data::createFromYAML(const char * string)
{
  yaml_parser_t parser;
  yaml_document_t document;
  if( yaml_parser_initialize(&parser) == 0 ) {
    throw Exception("Failed to initialize yaml parser");
  }
  
  const unsigned char * input = reinterpret_cast<const unsigned char *>(string);
  
  yaml_parser_set_input_string(&parser, input, strlen(string));
  if( 0 == yaml_parser_load(&parser, &document) ) {
    yaml_parser_delete(&parser);
    throw Exception("Failed to parse yaml document");
  }

  std::unique_ptr<Data> data(new Data());
  try {
    yaml_node_t * root = yaml_document_get_root_node(&document);
    if( root == NULL ) {
      throw Exception("Empty yaml document");
    }
    _createFromYAML(data.get(), &document, root);
  } catch( ... ) {
    yaml_document_delete(&document);
    yaml_parser_delete(&parser);
    throw;
  }

  yaml_document_delete(&document);
  yaml_parser_delete(&parser);

  return data.release();
}
#else
Data * Data::createFromYAML(const char * string)
{
  throw Exception("YAML support not enabled");
}
#endif


} // namespace Mustache
