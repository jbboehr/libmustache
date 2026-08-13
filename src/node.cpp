
#include "node.hpp"

#include "exception.hpp"
#include "utils.hpp"

#include <algorithm>
#include <limits>
#include <memory>

namespace mustache {

namespace {

const size_t serialHeaderSize = 14;
const size_t serialMaxSize = 64 * 1024 * 1024;
const size_t serialMaxDepth = 64;
const size_t serialMaxNodes = 100000;
const size_t serialMaxDataSize = 0x00ffffff;
const size_t serialMaxChildren = 0x0000ffff;
const size_t serialMaxDataPartsPerNode = 256;
const size_t serialMaxDataParts = 100000;

struct SerialState {
  SerialState() : nodes(0), dataParts(0) {}

  size_t nodes;
  size_t dataParts;
};

bool isSerializableType(Node::Type type)
{
  switch( type ) {
    case Node::TypeRoot:
    case Node::TypeOutput:
    case Node::TypeTag:
    case Node::TypeVariable:
    case Node::TypeNegate:
    case Node::TypeSection:
    case Node::TypeStop:
    case Node::TypeComment:
    case Node::TypePartial:
    case Node::TypeInlinePartial:
      return true;
    default:
      return false;
  }
}

bool typeAllowsChildren(Node::Type type)
{
  return (type & Node::TypeHasChildren) != 0;
}

void validateNodeShape(
    Node::Type type, size_t flags, bool hasData, size_t children)
{
  if( !isSerializableType(type) ) {
    throw Exception("Invalid serial node type");
  }
  if( (flags & ~static_cast<size_t>(Node::FlagEscape)) != 0 ) {
    throw Exception("Invalid serial node flags");
  }
  if( type == Node::TypeRoot ) {
    if( hasData ) {
      throw Exception("Invalid serial root data");
    }
  } else if( !hasData ) {
    throw Exception("Invalid serial node data");
  }
  if( children > 0 && !typeAllowsChildren(type) ) {
    throw Exception("Invalid serial node children");
  }
}

void checkSerialBudget(SerialState& state, size_t depth)
{
  if( depth >= serialMaxDepth ) {
    throw Exception("Serial node nesting limit exceeded");
  }
  if( state.nodes >= serialMaxNodes ) {
    throw Exception("Serial node count limit exceeded");
  }
  ++state.nodes;
}

void validateNodeData(
    SerialState& state, Node::Type type, const std::string& data)
{
  if( (type & Node::TypeHasDot) != 0 ) {
    const size_t parts =
        static_cast<size_t>(std::count(data.begin(), data.end(), '.')) + 1;
    if( parts > serialMaxDataPartsPerNode ||
        parts > serialMaxDataParts - state.dataParts ) {
      throw Exception("Serial data-part limit exceeded");
    }
    state.dataParts += parts;
  }
}

void ensureOutputSpace(const std::vector<uint8_t>& output, size_t additional)
{
  if( output.size() > serialMaxSize ||
      additional > serialMaxSize - output.size() ) {
    throw Exception("Serialized AST size limit exceeded");
  }
}

void appendByte(std::vector<uint8_t>& output, size_t value)
{
  ensureOutputSpace(output, 1);
  output.push_back(static_cast<uint8_t>(value));
}

void appendUint16(std::vector<uint8_t>& output, size_t value)
{
  appendByte(output, (value & 0x0000ff00) >> 8);
  appendByte(output, value & 0x000000ff);
}

void appendUint24(std::vector<uint8_t>& output, size_t value)
{
  appendByte(output, (value & 0x00ff0000) >> 16);
  appendByte(output, (value & 0x0000ff00) >> 8);
  appendByte(output, value & 0x000000ff);
}

void appendUint32(std::vector<uint8_t>& output, size_t value)
{
  appendByte(output, (value & 0xff000000) >> 24);
  appendByte(output, (value & 0x00ff0000) >> 16);
  appendByte(output, (value & 0x0000ff00) >> 8);
  appendByte(output, value & 0x000000ff);
}

void writeUint32(std::vector<uint8_t>& output, size_t pos, size_t value)
{
  output[pos + 0] = static_cast<uint8_t>((value & 0xff000000) >> 24);
  output[pos + 1] = static_cast<uint8_t>((value & 0x00ff0000) >> 16);
  output[pos + 2] = static_cast<uint8_t>((value & 0x0000ff00) >> 8);
  output[pos + 3] = static_cast<uint8_t>(value & 0x000000ff);
}

void serializeNode(const Node& node, std::vector<uint8_t>& output,
    SerialState& state, size_t depth)
{
  checkSerialBudget(state, depth);
  validateNodeShape(
      node.type, static_cast<size_t>(node.flags), node.data != NULL,
      node.children.size());

  if( node.flags < 0 ) {
    throw Exception("Invalid serial node flags");
  }
  if( node.children.size() > serialMaxChildren ) {
    throw Exception("Serial child count exceeds format limit");
  }

  size_t dataSize = 0;
  if( node.data != NULL ) {
    if( node.data->size() >= serialMaxDataSize ) {
      throw Exception("Serial node data exceeds format limit");
    }
    validateNodeData(state, node.type, *node.data);
    dataSize = node.data->size() + 1;
  }

  ensureOutputSpace(output, serialHeaderSize + dataSize);
  appendByte(output, 'M');
  appendByte(output, 'U');
  appendUint16(output, static_cast<size_t>(node.type));
  appendByte(output, static_cast<size_t>(node.flags));
  appendUint24(output, dataSize);
  appendUint16(output, node.children.size());

  const size_t childrenSizePos = output.size();
  appendUint32(output, 0);

  if( node.data != NULL ) {
    output.insert(output.end(), node.data->begin(), node.data->end());
    output.push_back(0);
  }

  const size_t childrenStart = output.size();
  for( Node::Children::const_iterator it = node.children.begin();
      it != node.children.end(); ++it ) {
    if( *it == NULL ) {
      throw Exception("Invalid null serial child");
    }
    serializeNode(**it, output, state, depth + 1);
  }

  const size_t childrenSize = output.size() - childrenStart;
  if( childrenSize > std::numeric_limits<uint32_t>::max() ) {
    throw Exception("Serial child data exceeds format limit");
  }
  writeUint32(output, childrenSizePos, childrenSize);
}

class SerialReader {
  public:
    SerialReader(const std::vector<uint8_t>& serial, size_t offset) :
        serial_(serial), pos_(offset) {}

    size_t position() const
    {
      return pos_;
    }

    size_t remaining(size_t limit) const
    {
      if( limit > serial_.size() || pos_ > limit ) {
        throw Exception("Invalid serial bounds");
      }
      return limit - pos_;
    }

    uint8_t readByte(size_t limit)
    {
      require(1, limit);
      return serial_[pos_++];
    }

    uint16_t readUint16(size_t limit)
    {
      uint16_t value = static_cast<uint16_t>(readByte(limit)) << 8;
      value |= static_cast<uint16_t>(readByte(limit));
      return value;
    }

    uint32_t readUint24(size_t limit)
    {
      uint32_t value = static_cast<uint32_t>(readByte(limit)) << 16;
      value |= static_cast<uint32_t>(readByte(limit)) << 8;
      value |= static_cast<uint32_t>(readByte(limit));
      return value;
    }

    uint32_t readUint32(size_t limit)
    {
      uint32_t value = static_cast<uint32_t>(readByte(limit)) << 24;
      value |= static_cast<uint32_t>(readByte(limit)) << 16;
      value |= static_cast<uint32_t>(readByte(limit)) << 8;
      value |= static_cast<uint32_t>(readByte(limit));
      return value;
    }

    std::string readString(size_t length, size_t limit)
    {
      require(length, limit);
      std::string value;
      if( length > 0 ) {
        value.assign(reinterpret_cast<const char *>(&serial_[pos_]), length);
        pos_ += length;
      }
      return value;
    }

  private:
    void require(size_t length, size_t limit) const
    {
      if( length > remaining(limit) ) {
        throw Exception("Truncated serial data");
      }
    }

    const std::vector<uint8_t>& serial_;
    size_t pos_;
};

std::unique_ptr<Node> unserializeNode(SerialReader& reader, size_t limit,
    SerialState& state, size_t depth)
{
  checkSerialBudget(state, depth);

  if( reader.remaining(limit) < serialHeaderSize ) {
    throw Exception("Truncated serial header");
  }
  if( reader.readByte(limit) != 'M' || reader.readByte(limit) != 'U' ) {
    throw Exception("Invalid serial magic");
  }

  const Node::Type type = static_cast<Node::Type>(reader.readUint16(limit));
  const size_t flags = reader.readByte(limit);
  const size_t dataSize = reader.readUint24(limit);
  const size_t childrenNum = reader.readUint16(limit);
  const size_t childrenSize = reader.readUint32(limit);

  validateNodeShape(type, flags, dataSize > 0, childrenNum);
  if( dataSize > reader.remaining(limit) ) {
    throw Exception("Truncated serial node data");
  }

  std::string data;
  if( dataSize > 0 ) {
    data = reader.readString(dataSize - 1, limit);
    if( reader.readByte(limit) != 0 ) {
      throw Exception("Invalid serial data terminator");
    }
    validateNodeData(state, type, data);
  }

  if( childrenSize > reader.remaining(limit) ) {
    throw Exception("Truncated serial child data");
  }
  if( childrenNum > 0 && childrenSize / serialHeaderSize < childrenNum ) {
    throw Exception("Invalid serial child size");
  }
  const size_t childrenEnd = reader.position() + childrenSize;

  std::unique_ptr<Node> node(new Node());
  node->type = type;
  node->flags = static_cast<int>(flags);
  if( dataSize > 0 ) {
    node->setData(data);
  }
  if( type == Node::TypeSection ) {
    node->startSequence = new std::string("{{");
    node->stopSequence = new std::string("}}");
  }

  node->children.reserve(childrenNum);
  for( size_t i = 0; i < childrenNum; ++i ) {
    std::unique_ptr<Node> child(
        unserializeNode(reader, childrenEnd, state, depth + 1));
    node->children.push_back(child.get());
    child.release();
  }

  if( reader.position() != childrenEnd ) {
    throw Exception("Invalid serial child size");
  }
  return node;
}

} // namespace


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
  std::unique_ptr<std::vector<uint8_t> > output(
      new std::vector<uint8_t>());
  output->reserve(18);

  SerialState state;
  serializeNode(*this, *output, state, 0);
  return output.release();
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
  if( vpos == NULL ) {
    throw Exception("Missing serial output position");
  }
  if( offset > serial.size() ) {
    throw Exception("Invalid serial offset");
  }
  if( serial.size() - offset > serialMaxSize ) {
    throw Exception("Serialized AST size limit exceeded");
  }

  SerialReader reader(serial, offset);
  SerialState state;
  std::unique_ptr<Node> node(
      unserializeNode(reader, serial.size(), state, 0));
  if( reader.position() != serial.size() ) {
    throw Exception("Trailing serial data");
  }

  *vpos = reader.position();
  return node.release();
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
