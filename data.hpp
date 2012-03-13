
#ifndef MUSTACHE_DATA_HPP
#define MUSTACHE_DATA_HPP

#include <list>
#include <memory>
#include <map>
#include <string>
#include <vector>

#include "exception.hpp"

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
    typedef std::map<std::string,Data *> Map;
    typedef std::list<Data *> List;
    typedef Data * Array;
    
    //! Enum of the different supported data types
    enum Type { 
      TypeNone = 0,
      TypeString = 1,
      TypeList = 2,
      TypeMap = 3,
      TypeArray = 4
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
    
    //! Destructor
    ~Data();
    
    //! Checks if the node is empty. The includes an empty value.
    int isEmpty();
    
    //! Initialize the data
    void init(Data::Type type, int size);
};

/*! \class Stack
    \brief Data stack.

    This class is used to implement stack lookups in the renderer.
*/
class Stack {
  public:
    //! The maximum size of the stack
    static const int MAXSIZE = 100;
    
    //! Constructor
    Stack() : size(0) {};
    
    //! The current size
    int size;
    
    //! The data
    Data * stack[Stack::MAXSIZE];
    
    //! Add an element onto the top of the stack
    void push(Data * data);
    
    //! Pop an element off the top of the stack
    void pop();
    
    //! Get the top of the stack
    Data * top();
    
    //! Gets a pointer to the beginning of the stack
    Data ** begin();
    
    //! Gets a pointer to the end of the stack
    Data ** end();
};


} // namespace Mustache

#endif


