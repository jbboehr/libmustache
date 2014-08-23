
#include "data.hpp"

#ifdef MUSTACHE_HAVE_LIBYAML
#include "yaml.h"
#endif

#if defined(MUSTACHE_HAVE_LIBJSON)
#include "json/json.h"
//#include "json/json_inttypes.h"
#include "json/json_object.h"
#include "json/json_tokener.h"
#elif defined(MUSTACHE_HAVE_LIBJANSSON)
#include "jansson.h"
#endif

#include "stdio.h"

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
      //delete[] array;
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





Data * searchStack(Stack<Data *> * stack, std::string * key)
{
  // Resolve up the data stack
  Data * ref = NULL;
  Data::Map::iterator d_it;
  register int i;
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
  Data * child = NULL;
  
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
        child = new Data();
        _createFromJSON(child, value);
        data->data.insert(std::pair<std::string,Data *>(ckey,child));
      }
      break;
    }
    case json_type_array: {
      int len = json_object_array_length(object);
      data->init(Data::TypeArray, len);
      //child = data->array;
      
      struct json_object * array_item;
      for( int i = 0; i < len; i++, child++ ) {
        //array_item = json_object_array_get_idx(object, i);
        //_createFromJSON(child, array_item);
      }
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
  Data * data = new Data();
  data->type = Data::TypeNone;
  _createFromJSON(data, result);
  json_object_put(result);
  return data;
}
#elif defined(MUSTACHE_HAVE_LIBJANNSON)
Data * Data::createFromJSON(const char * string)
{
  throw Exception("JSON support using libjannson not implemented");
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
  Data * child = NULL;
  
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
        child = new Data();
        _createFromYAML(child, document, valueNode);
        data->data.insert(std::pair<std::string,Data *>(ckey,child));
      }
      break;
    }
    case YAML_SEQUENCE_NODE: {
      int len = (node->data.sequence.items.top - node->data.sequence.items.start);
      data->init(Data::TypeArray, len);
      //child = data->array;
      
      yaml_node_item_t * item;
      for( item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++, child++) {
        //yaml_node_t * valueNode = yaml_document_get_node(document, *item);
        //_createFromYAML(child, document, valueNode);
      }
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
  yaml_parser_initialize(&parser);
  
  const unsigned char * input = reinterpret_cast<const unsigned char *>(string);
  
  yaml_parser_set_input_string(&parser, input, strlen(string));
  yaml_parser_load(&parser, &document);
  
  Data * data = new Data();
  data->type = Data::TypeNone;
  _createFromYAML(data, &document, yaml_document_get_root_node(&document));
  
  yaml_document_delete(&document);
  yaml_parser_delete(&parser);
  
  return data;
}
#else
Data * Data::createFromYAML(const char * string)
{
  throw Exception("YAML support not enabled");
}
#endif


} // namespace Mustache
