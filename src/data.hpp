
#ifndef MUSTACHE_DATA_HPP
#define MUSTACHE_DATA_HPP

#include "mustache_config.h"

#include <list>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

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
    typedef std::string String;
    typedef std::unordered_map<std::string,Data *> Map;
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
        length(0),
        val(NULL),
        lambda(NULL) {}
    Data(Data::Type type, int size) :
        type(Data::TypeNone),
        length(0),
        val(NULL),
        lambda(NULL) {
      init(type, size);
    }

    // Data owns its active value and must never be shallow-copied.
    Data(const Data&) = delete;
    Data& operator=(const Data&) = delete;
    Data(Data&& other) noexcept;
    Data& operator=(Data&& other) noexcept;
    
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

  private:
    void swap(Data& other) noexcept;
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
