
#ifndef MUSTACHE_DATA_HPP
#define MUSTACHE_DATA_HPP

//#ifdef HAVE_CONFIG_H
#include "mustache_config.h"
//#endif

#include <list>
#include <memory>
#include <map>
#include <string>
#include <vector>

#include MUSTACHE_HASH_MAP_H

#include "exception.hpp"
#include "lambda.hpp"
#include "stack.hpp"

namespace mustache {

/*! \class Data
    \brief Associative data structure.

    This class implements a data structure similar to associative arrays in PHP
    or JSON.
*/
class Data {
  public:
    typedef std::auto_ptr<Data> Ptr;
    typedef std::string String;
    typedef MUSTACHE_HASH_NAMESPACE::MUSTACHE_HASH_MAP_CLASS<std::string,Data *> Map;
    typedef std::list<Data *> List;
    typedef std::vector<Data *> Array;
    
    //! Enum of the different supported data types
    enum Type { 
      TypeNone = 0,
      TypeString = 1,
      TypeList = 2,
      TypeMap = 3,
      TypeArray = 4,
      TypeLambda = 5
    };
    
    //! The data type
    Data::Type type;
    
    //! The length of the data (only used for the array type)
    int length;
    
    //! The current string value
    Data::String * val;
    
    //! The current map value
    Data::Map data;
    
    //! The current array value (list)
    Data::List children;
    
    //! The current array value (array)
    Data::Array array;

    //! The curent lambda value
    Lambda * lambda;
    
    //! Constructor
    Data() : 
        type(Data::TypeNone),
        val(NULL),
        lambda(NULL) {};
    Data(Data::Type type, int size) : val(NULL), lambda(NULL) {
      init(type, size);
    };
    
    //! Destructor
    ~Data();
    
    //! Checks if the node is empty. The includes an empty value.
    int isEmpty();
    
    //! Initialize the data
    void init(Data::Type type, int size);
    
    //! Create from json
    static Data * createFromJSON(const char * string);
    
    //! Create from yaml
    static Data * createFromYAML(const char * string);
};

Data * searchStack(Stack<Data *> * stack, std::string * key);

Data * searchStackNR(Stack<Data *> * stack, std::string * key);

/*! \class DataStack
    \brief Data stack.

    This class is used to implement stack lookups in the renderer.
class DataStack : Stack<Data *> {
    
    //! Looks up the stack for a map value
    Data * search(std::string * key);
    
    Data * searchnr(std::string * key);
};
*/


} // namespace Mustache

#endif


