
#include "node.hpp"

#include "exception.hpp"
#include "utils.hpp"

namespace mustache {


Node::~Node()
{
  // Data
  if( data != NULL ) {
    delete data;
  }
  
  // Data parts
  if( dataParts != NULL ) {
    delete dataParts;
  }
  
  // Children
  if( children.size() > 0 ) {
    Node::Children::iterator it;
    for ( it = children.begin() ; it != children.end(); it++ ) {
      delete *it;
    }
  }
  children.clear();
  
  // Child should not be freed

  if( startSequence != NULL ) {
    delete startSequence;
  }

  if( stopSequence != NULL ) {
    delete stopSequence;
  }
}

std::string Node::children_to_template_string(const std::string& start, const std::string& stop)
{
  std::string template_string;

  if( children.size() > 0 ) {
    Node::Children::iterator it;
    for( it = children.begin() ; it != children.end(); it++ ) {
      if( (*it)->type == Node::TypeStop ) {
        continue;
      }

      template_string.append((*it)->to_template_string(start, stop));
    }
  }

  return template_string;
}

void Node::setData(const std::string& data)
{
  this->data = new std::string(data);
  
  if( this->type & Node::TypeHasDot ) {
    size_t found = data.find(".");
    if( found != std::string::npos ) {
      dataParts = new std::vector<std::string>;
      explode(".", *(this->data), dataParts);
    }
  }
}

std::vector<uint8_t> * Node::serialize()
{
  // Calculate size
  size_t size = 0;
  std::vector<uint8_t> * ret = new std::vector<uint8_t>;
  std::vector<uint8_t> & retr = *ret;
  
  // Reserve header size
  retr.reserve(18);
  
  // Add header
  retr.push_back('M');
  retr.push_back('U');
  
  // Boundary
  //retr.push_back(0xff);
  
  // Add type
  //retr.push_back((this->type & 0xff000000) >> 24);
  //retr.push_back((this->type & 0x00ff0000) >> 16);
  retr.push_back((this->type & 0x0000ff00) >> 8);
  retr.push_back((this->type & 0x000000ff));
  
  // Boundary
  //retr.push_back(0xfe);
  
  // Add flagx
  //retr.push_back((this->flags & 0xff000000) >> 24);
  //retr.push_back((this->flags & 0x00ff0000) >> 16);
  //retr.push_back((this->flags & 0x0000ff00) >> 8);
  retr.push_back((this->flags & 0x000000ff));
  
  // Boundary
  //retr.push_back(0xfd);
  
  // Add size of data
  size_t data_size = 0;
  if( this->data != NULL ) {
    data_size = this->data->size() + 1;
  }
  //retr.push_back((data_size & 0xff000000) >> 24);
  retr.push_back((data_size & 0x00ff0000) >> 16);
  retr.push_back((data_size & 0x0000ff00) >> 8);
  retr.push_back((data_size & 0x000000ff));
  
  // Boundary
  //retr.push_back(0xfc);
  
  // Add number of children
  size_t n_children = children.size();
  //retr.push_back((n_children & 0xff000000) >> 24);
  //retr.push_back((n_children & 0x00ff0000) >> 16);
  retr.push_back((n_children & 0x0000ff00) >> 8);
  retr.push_back((n_children & 0x000000ff));
  
  // Boundary
  //retr.push_back(0xfb);
  
  // Initialize children size
  size_t children_size_pos = retr.size();
  size_t children_size = 0;
  retr.push_back(0x00);
  retr.push_back(0x00);
  retr.push_back(0x00);
  retr.push_back(0x00);
  
  // Boundary
  //retr.push_back(0xfa);
  
  // Add data
  if( this->data != NULL ) {
    retr.insert(retr.end(), this->data->begin(), this->data->end());
    retr.push_back(0x00); // null terminate for paranoia
  }
  
  // Boundary
  //retr.push_back(0xf9);
  
  // Add children
  if( n_children > 0 ) {
    Node::Children::iterator it;
    for ( it = children.begin() ; it != children.end(); it++ ) {
      std::vector<uint8_t> * cret = (*it)->serialize();
      std::vector<uint8_t> & cretr = *cret;
      children_size += cretr.size();
      retr.insert(retr.end(), cretr.begin(), cretr.end());
      delete cret;
    }
  }
  
  // Set children size
  retr[children_size_pos + 0] = ((children_size & 0xff000000) >> 24);
  retr[children_size_pos + 1] = ((children_size & 0x00ff0000) >> 16);
  retr[children_size_pos + 2] = ((children_size & 0x0000ff00) >> 8);
  retr[children_size_pos + 3] = ((children_size & 0x000000ff));
  
  // Boundary
  //retr.push_back(0xf8);
  
  return ret;
}

std::string Node::to_template_string(const std::string& start, const std::string& stop)
{
  std::string template_string;

  switch( type ) {
    case Node::TypeComment:
      template_string.append(start);
      template_string.append("!");
      template_string.append(*data);
      template_string.append(stop);
      break;
    case Node::TypeOutput:
      template_string.assign(*data);
      break;
    case Node::TypePartial:
      template_string.append(start);
      template_string.append(">");
      template_string.append(*data);
      template_string.append(stop);
      break;
    case Node::TypeNegate:
    case Node::TypeSection:
    case Node::TypeStop:
    case Node::TypeVariable:
      template_string.append(start);

      if( type == Node::TypeVariable && !(flags & Node::FlagEscape) ) {
        template_string.append("&");
      }

      switch( type ) {
        case Node::TypeNegate:
          template_string.append("^");
          break;
        case Node::TypeSection:
          template_string.append("#");
          break;
        case Node::TypeStop:
          template_string.append("/");
          break;
      }

      template_string.append(*data);

      template_string.append(stop);
    case Node::TypeRoot: // a root node only has children, so start here
      if( children.size() > 0 ) {
        Node::Children::iterator it;
        for( it = children.begin() ; it != children.end(); it++ ) {
          template_string.append((*it)->to_template_string(start, stop));
        }
      }
      break;
  }

  return template_string;
}

Node * Node::unserialize(std::vector<uint8_t>& serial, size_t offset, size_t * vpos)
{
  size_t pos = offset;
  
  if( serial.size() - pos < 2 || serial[pos++] != 'M' || serial[pos++] != 'U' ) {
    throw Exception("Invalid serial data");
  }
  
  // Boundary
  //pos++;
  
  // Type
  size_t type = 0;
  //type += (serial[pos++] << 24);
  //type += (serial[pos++] << 16);
  type += (serial[pos++] << 8);
  type += (serial[pos++]);
  
  // Boundary
  //pos++;
  
  // Flags
  size_t flags = 0;
  //flags += (serial[pos++] << 24);
  //flags += (serial[pos++] << 16);
  //flags += (serial[pos++] << 8);
  flags += (serial[pos++]);
  
  // Boundary
  //pos++;
  
  // Size of data
  size_t data_size = 0;
  //data_size += (serial[pos++] << 24);
  data_size += (serial[pos++] << 16);
  data_size += (serial[pos++] << 8);
  data_size += (serial[pos++]);
  
  // Boundary
  //pos++;
  
  // Number of children
  size_t children_num = 0;
  //children_num += (serial[pos++] << 24);
  //children_num += (serial[pos++] << 16);
  children_num += (serial[pos++] << 8);
  children_num += (serial[pos++]);
  
  // Boundary
  //pos++;
  
  // Children size
  size_t children_size = 0;
  children_size += (serial[pos++] << 24);
  children_size += (serial[pos++] << 16);
  children_size += (serial[pos++] << 8);
  children_size += (serial[pos++]);
  
  // Boundary
  //pos++;
  
  // Data
  std::string data;
  if( data_size > 0 ) {
    data.resize(data_size - 1);
    int i = 0;
    for( i = 0; i < data_size - 1; i++ ) {
      data[i] = serial[pos++];
    }
    pos++; // null byte?
  }
  
  // Boundary
  //pos++;
  
  *vpos = pos;
  
  // Initialize main node
  Node * node = new Node();
  node->type = static_cast<Node::Type>(type);
  node->flags = flags;
  if( data.size() > 0 ) {
    node->setData(data);
  }
  
  // Children
  if( children_num > 0 ) {
    node->children.resize(children_num);
    for( int i = 0; i < children_num; i++ ) {
      Node * child = Node::unserialize(serial, *vpos, vpos);
      node->children[i] = child;
    }
  }
  
  //pos = *vpos;
  
  // Boundary
  //pos++;
  
  //*vpos = pos;
  
  return node;
}



void NodeStack::push_back(Node * node)
{
  if( _size < 0 || _size >= NodeStack::MAXSIZE ) {
    throw Exception("Reached max stack size");
  }
  _stack[_size] = node;
  _size++;
}

void NodeStack::pop_back()
{
  if( _size > 0 ) {
    _size--;
    _stack[_size] = NULL;
  }
}

Node * NodeStack::back()
{
  if( _size <= 0 ) {
    throw Exception("Reached bottom of stack");
  } else {
    return _stack[_size - 1];
  }
}

Node ** NodeStack::begin()
{
  return _stack;
}

Node ** NodeStack::end()
{
  return (_stack + _size - 1);
}


} // namespace Mustache
