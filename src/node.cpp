
#include "node.hpp"

#include "exception.hpp"
#include "utils.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

namespace mustache {

namespace {

const size_t serialHeaderSize = 14;
const size_t serialMaxDataSize = 0x00ffffff;
const size_t serialMaxChildren = 0x0000ffff;
const size_t templateStringImplementationMaxDepth = 256;

struct SerialState {
    explicit SerialState(const Node::SerializationLimits& limits) :
        limits(limits),
        nodes(0),
        dataParts(0)
    {}

    const Node::SerializationLimits& limits;
    size_t nodes;
    size_t dataParts;
};

struct TemplateStringState {
    explicit TemplateStringState(const Node::TemplateStringLimits& limits) :
        limits(limits),
        nodes(0)
    {}

    const Node::TemplateStringLimits& limits;
    size_t nodes;
};

bool isSerializableTypeValue(size_t type)
{
  switch (type) {
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

bool isSerializableType(Node::Type type)
{
  return isSerializableTypeValue(static_cast<size_t>(type));
}

bool typeAllowsChildren(Node::Type type)
{
  return (type & Node::TypeHasChildren) != 0;
}

void validateNodeShape(Node::Type type, size_t flags, bool hasData, size_t children)
{
  if (!isSerializableType(type)) {
    throw Exception("Invalid serial node type");
  }
  const size_t validFlags = static_cast<size_t>(Node::FlagEscape) | static_cast<size_t>(Node::FlagLambdaOnly) |
      static_cast<size_t>(Node::FlagPartialIndent);
  if ((flags & ~validFlags) != 0 ||
      ((flags & static_cast<size_t>(Node::FlagLambdaOnly)) != 0 &&
          (type != Node::TypeOutput ||
              (flags != static_cast<size_t>(Node::FlagLambdaOnly) &&
                  flags !=
                      (static_cast<size_t>(Node::FlagLambdaOnly) | static_cast<size_t>(Node::FlagPartialIndent))))) ||
      ((flags & static_cast<size_t>(Node::FlagPartialIndent)) != 0 &&
          (type != Node::TypeOutput ||
              flags != (static_cast<size_t>(Node::FlagLambdaOnly) | static_cast<size_t>(Node::FlagPartialIndent))))) {
    throw Exception("Invalid serial node flags");
  }
  if (type == Node::TypeRoot) {
    if (hasData) {
      throw Exception("Invalid serial root data");
    }
  } else if (!hasData) {
    throw Exception("Invalid serial node data");
  }
  if (children > 0 && !typeAllowsChildren(type)) {
    throw Exception("Invalid serial node children");
  }
}

bool isPartialIndentationMetadata(const Node& node)
{
  return node.type == Node::TypeOutput && node.flags == (Node::FlagLambdaOnly | Node::FlagPartialIndent) &&
      node.data.has_value() && std::all_of(node.data->begin(), node.data->end(), [](char value) {
        return value == ' ' || value == '\t';
      });
}

void validatePartialIndentationChildren(const Node::Children& children)
{
  for (size_t index = 0; index < children.size(); ++index) {
    const Node * child = children[index].get();
    if (child == NULL || (child->flags & Node::FlagPartialIndent) == 0) {
      continue;
    }
    if (!isPartialIndentationMetadata(*child) || index + 1 >= children.size() || children[index + 1] == NULL ||
        children[index + 1]->type != Node::TypePartial) {
      throw Exception("Invalid serial partial indentation metadata");
    }
    ++index;
  }
}

void checkSerialBudget(SerialState& state, size_t depth)
{
  if (depth >= state.limits.maxNestingDepth) {
    throw Exception("Serial node nesting limit exceeded");
  }
  if (state.nodes >= state.limits.maxNodes) {
    throw Exception("Serial node count limit exceeded");
  }
  ++state.nodes;
}

void validateNodeData(SerialState& state, Node::Type type, const std::string& data)
{
  if ((type & Node::TypeHasDot) != 0) {
    const size_t parts = static_cast<size_t>(std::count(data.begin(), data.end(), '.')) + 1;
    if (parts > state.limits.maxDataPartsPerNode || state.dataParts > state.limits.maxDataParts ||
        parts > state.limits.maxDataParts - state.dataParts) {
      throw Exception("Serial data-part limit exceeded");
    }
    state.dataParts += parts;
  }
}

void ensureOutputSpace(const std::vector<uint8_t>& output, size_t additional, const SerialState& state)
{
  if (output.size() > state.limits.maxOutputBytes || additional > state.limits.maxOutputBytes - output.size()) {
    throw Exception("Serialized AST size limit exceeded");
  }
}

void appendByte(std::vector<uint8_t>& output, size_t value, const SerialState& state)
{
  ensureOutputSpace(output, 1, state);
  output.push_back(static_cast<uint8_t>(value));
}

void appendUint16(std::vector<uint8_t>& output, size_t value, const SerialState& state)
{
  appendByte(output, (value & 0x0000ff00) >> 8, state);
  appendByte(output, value & 0x000000ff, state);
}

void appendUint24(std::vector<uint8_t>& output, size_t value, const SerialState& state)
{
  appendByte(output, (value & 0x00ff0000) >> 16, state);
  appendByte(output, (value & 0x0000ff00) >> 8, state);
  appendByte(output, value & 0x000000ff, state);
}

void appendUint32(std::vector<uint8_t>& output, size_t value, const SerialState& state)
{
  appendByte(output, (value & 0xff000000) >> 24, state);
  appendByte(output, (value & 0x00ff0000) >> 16, state);
  appendByte(output, (value & 0x0000ff00) >> 8, state);
  appendByte(output, value & 0x000000ff, state);
}

void writeUint32(std::vector<uint8_t>& output, size_t pos, size_t value)
{
  output[pos + 0] = static_cast<uint8_t>((value & 0xff000000) >> 24);
  output[pos + 1] = static_cast<uint8_t>((value & 0x00ff0000) >> 16);
  output[pos + 2] = static_cast<uint8_t>((value & 0x0000ff00) >> 8);
  output[pos + 3] = static_cast<uint8_t>(value & 0x000000ff);
}

void serializeNode(
    const Node& node, std::vector<uint8_t>& output, SerialState& state, size_t depth, bool partialIndentationMetadata)
{
  checkSerialBudget(state, depth);
  validateNodeShape(node.type, static_cast<size_t>(node.flags), node.data.has_value(), node.children.size());
  if ((node.flags & Node::FlagPartialIndent) != 0 && !partialIndentationMetadata) {
    throw Exception("Invalid serial partial indentation metadata");
  }
  validatePartialIndentationChildren(node.children);

  if (node.flags < 0) {
    throw Exception("Invalid serial node flags");
  }
  if (node.children.size() > serialMaxChildren) {
    throw Exception("Serial child count exceeds format limit");
  }

  size_t dataSize = 0;
  if (node.data.has_value()) {
    if (node.data->size() >= serialMaxDataSize) {
      throw Exception("Serial node data exceeds format limit");
    }
    validateNodeData(state, node.type, *node.data);
    dataSize = node.data->size() + 1;
  }

  ensureOutputSpace(output, serialHeaderSize + dataSize, state);
  appendByte(output, 'M', state);
  appendByte(output, 'U', state);
  appendUint16(output, static_cast<size_t>(node.type), state);
  appendByte(output, static_cast<size_t>(node.flags), state);
  appendUint24(output, dataSize, state);
  appendUint16(output, node.children.size(), state);

  const size_t childrenSizePos = output.size();
  appendUint32(output, 0, state);

  if (node.data.has_value()) {
    output.insert(output.end(), node.data->begin(), node.data->end());
    output.push_back(0);
  }

  const size_t childrenStart = output.size();
  for (Node::Children::const_iterator it = node.children.begin(); it != node.children.end(); ++it) {
    if (*it == NULL) {
      throw Exception("Invalid null serial child");
    }
    serializeNode(**it, output, state, depth + 1, isPartialIndentationMetadata(**it));
  }

  const size_t childrenSize = output.size() - childrenStart;
  if (childrenSize > std::numeric_limits<uint32_t>::max()) {
    throw Exception("Serial child data exceeds format limit");
  }
  writeUint32(output, childrenSizePos, childrenSize);
}

class SerialReader {
  public:
    SerialReader(const uint8_t * serial, size_t size, size_t offset) :
        serial_(serial),
        size_(size),
        pos_(offset)
    {}

    size_t position() const
    {
      return pos_;
    }

    size_t remaining(size_t limit) const
    {
      if (limit > size_ || pos_ > limit) {
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
      if (length > 0) {
        value.assign(reinterpret_cast<const char *>(serial_ + pos_), length);
        pos_ += length;
      }
      return value;
    }

  private:
    void require(size_t length, size_t limit) const
    {
      if (length > remaining(limit)) {
        throw Exception("Truncated serial data");
      }
    }

    const uint8_t * serial_;
    size_t size_;
    size_t pos_;
};

std::unique_ptr<Node> unserializeNode(SerialReader& reader, size_t limit, SerialState& state, size_t depth)
{
  checkSerialBudget(state, depth);

  if (reader.remaining(limit) < serialHeaderSize) {
    throw Exception("Truncated serial header");
  }
  if (reader.readByte(limit) != 'M' || reader.readByte(limit) != 'U') {
    throw Exception("Invalid serial magic");
  }

  const size_t typeValue = reader.readUint16(limit);
  if (!isSerializableTypeValue(typeValue)) {
    throw Exception("Invalid serial node type");
  }
  const Node::Type type = static_cast<Node::Type>(typeValue);
  const size_t flags = reader.readByte(limit);
  const size_t dataSize = reader.readUint24(limit);
  const size_t childrenNum = reader.readUint16(limit);
  const size_t childrenSize = reader.readUint32(limit);

  validateNodeShape(type, flags, dataSize > 0, childrenNum);
  if (dataSize > reader.remaining(limit)) {
    throw Exception("Truncated serial node data");
  }

  std::string data;
  if (dataSize > 0) {
    data = reader.readString(dataSize - 1, limit);
    if (reader.readByte(limit) != 0) {
      throw Exception("Invalid serial data terminator");
    }
    validateNodeData(state, type, data);
  }

  if (childrenSize > reader.remaining(limit)) {
    throw Exception("Truncated serial child data");
  }
  if (childrenNum > 0 && childrenSize / serialHeaderSize < childrenNum) {
    throw Exception("Invalid serial child size");
  }
  const size_t childrenEnd = reader.position() + childrenSize;

  std::unique_ptr<Node> node = std::make_unique<Node>();
  node->type = type;
  node->flags = static_cast<int>(flags);
  if (dataSize > 0) {
    node->setData(data);
  }
  if (type == Node::TypeSection) {
    node->startSequence = "{{";
    node->stopSequence = "}}";
  }

  node->children.reserve(childrenNum);
  for (size_t i = 0; i < childrenNum; ++i) {
    node->children.push_back(unserializeNode(reader, childrenEnd, state, depth + 1));
  }

  validatePartialIndentationChildren(node->children);

  if (reader.position() != childrenEnd) {
    throw Exception("Invalid serial child size");
  }
  return node;
}

std::unique_ptr<Node> unserializeOwnedRange(
    const uint8_t * serial, size_t length, size_t offset, const Node::SerializationLimits& limits)
{
  if (serial == NULL && length != 0) {
    throw Exception("Missing serial data");
  }
  if (offset > length) {
    throw Exception("Invalid serial offset");
  }
  if (length - offset > limits.maxInputBytes) {
    throw Exception("Serialized AST size limit exceeded");
  }

  SerialReader reader(serial, length, offset);
  SerialState state(limits);
  std::unique_ptr<Node> node(unserializeNode(reader, length, state, 0));
  if ((node->flags & Node::FlagPartialIndent) != 0) {
    throw Exception("Invalid serial partial indentation metadata");
  }
  if (reader.position() != length) {
    throw Exception("Trailing serial data");
  }
  return node;
}

void checkTemplateStringBudget(TemplateStringState& state, size_t depth)
{
  if (depth >= state.limits.maxNestingDepth || depth >= templateStringImplementationMaxDepth) {
    throw Exception("Template node nesting limit exceeded");
  }
  if (state.nodes >= state.limits.maxNodes) {
    throw Exception("Template node count limit exceeded");
  }
  ++state.nodes;
}

void appendTemplateString(std::string& output, std::string_view value, const TemplateStringState& state)
{
  if (output.size() > state.limits.maxOutputBytes || value.size() > state.limits.maxOutputBytes - output.size()) {
    throw Exception("Template reconstruction size limit exceeded");
  }
  output.append(value.data(), value.size());
}

void appendNodeTemplate(const Node& node, const std::string& start, const std::string& stop, std::string& output,
    TemplateStringState& state, size_t depth);

void appendNodeChildren(const Node& node, const std::string& start, const std::string& stop, std::string& output,
    TemplateStringState& state, size_t depth, bool skipStops)
{
  for (Node::Children::const_iterator it = node.children.begin(); it != node.children.end(); ++it) {
    if (*it == NULL) {
      throw Exception("Invalid null child node");
    }
    if (skipStops && (*it)->type == Node::TypeStop) {
      checkTemplateStringBudget(state, depth);
      continue;
    }
    appendNodeTemplate(**it, start, stop, output, state, depth);
  }
}

const std::string& requireNodeData(const Node& node)
{
  if (!node.data.has_value()) {
    throw Exception("Invalid node without data");
  }
  return node.data.value();
}

void appendNodeTemplate(const Node& node, const std::string& start, const std::string& stop, std::string& output,
    TemplateStringState& state, size_t depth)
{
  checkTemplateStringBudget(state, depth);

  if (!(node.type & Node::TypeHasNoString) && !node.data.has_value()) {
    throw Exception("Invalid node without data");
  }

  switch (node.type) {
    case Node::TypeComment:
      appendTemplateString(output, start, state);
      appendTemplateString(output, "!", state);
      appendTemplateString(output, requireNodeData(node), state);
      appendTemplateString(output, stop, state);
      break;
    case Node::TypeOutput:
      appendTemplateString(output, requireNodeData(node), state);
      break;
    case Node::TypePartial:
      appendTemplateString(output, start, state);
      appendTemplateString(output, ">", state);
      appendTemplateString(output, requireNodeData(node), state);
      appendTemplateString(output, stop, state);
      break;
    case Node::TypeNegate:
    case Node::TypeSection:
    case Node::TypeStop:
    case Node::TypeVariable:
      appendTemplateString(output, start, state);
      if (node.type == Node::TypeVariable && !(node.flags & Node::FlagEscape)) {
        appendTemplateString(output, "&", state);
      }
      if (node.type == Node::TypeNegate) {
        appendTemplateString(output, "^", state);
      } else if (node.type == Node::TypeSection) {
        appendTemplateString(output, "#", state);
      } else if (node.type == Node::TypeStop) {
        appendTemplateString(output, "/", state);
      }
      appendTemplateString(output, requireNodeData(node), state);
      appendTemplateString(output, stop, state);
      [[fallthrough]];
    case Node::TypeRoot:
      appendNodeChildren(node, start, stop, output, state, depth + 1, false);
      break;
    case Node::TypeNone:
    case Node::TypeTag:
    case Node::TypeContainer:
    case Node::TypeInlinePartial:
    case Node::TypeHasChildren:
    case Node::TypeHasData:
    case Node::TypeHasNoString:
    case Node::TypeHasDot:
      break;
  }
}

} // namespace

Node::SerializationLimits::SerializationLimits() :
    maxInputBytes(std::size_t{64} * 1024 * 1024),
    maxOutputBytes(std::size_t{64} * 1024 * 1024),
    maxNestingDepth(64),
    maxNodes(100000),
    maxDataPartsPerNode(256),
    maxDataParts(100000)
{}

Node::TemplateStringLimits::TemplateStringLimits() :
    maxOutputBytes(std::size_t{64} * 1024 * 1024),
    maxNestingDepth(64),
    // Tokenizer::Limits::maxNodes excludes the root receiver.
    maxNodes(100001)
{}

Node::Node(Node&& other) noexcept :
    type(other.type),
    flags(other.flags),
    data(std::move(other.data)),
    dataParts(std::move(other.dataParts)),
    children(std::move(other.children)),
    child(std::move(other.child)),
    partials(std::move(other.partials)),
    startSequence(std::move(other.startSequence)),
    stopSequence(std::move(other.stopSequence))
{
  other.resetMovedFrom();
}

Node& Node::operator=(Node&& other) noexcept
{
  if (this != &other) {
    type = other.type;
    flags = other.flags;
    data = std::move(other.data);
    dataParts = std::move(other.dataParts);
    children = std::move(other.children);
    child = std::move(other.child);
    partials = std::move(other.partials);
    startSequence = std::move(other.startSequence);
    stopSequence = std::move(other.stopSequence);
    other.resetMovedFrom();
  }
  return *this;
}

void Node::resetMovedFrom() noexcept
{
  type = Node::TypeNone;
  flags = Node::FlagNone;
  data.reset();
  dataParts.clear();
  children.clear();
  child.reset();
  partials.clear();
  startSequence.reset();
  stopSequence.reset();
}

Node::~Node() = default;

std::string Node::children_to_template_string(const std::string& start, const std::string& stop) const
{
  return children_to_template_string(start, stop, TemplateStringLimits());
}

std::string Node::children_to_template_string(
    const std::string& start, const std::string& stop, const TemplateStringLimits& limits) const
{
  std::string output;
  TemplateStringState state(limits);
  checkTemplateStringBudget(state, 0);
  appendNodeChildren(*this, start, stop, output, state, 1, true);
  return output;
}

void Node::setData(const std::string& value)
{
  std::optional<std::string> nextData(value);
  std::vector<std::string> nextDataParts;

  if (this->type & Node::TypeHasDot) {
    size_t found = value.find('.');
    if (found != std::string::npos) {
      explode(".", value, &nextDataParts);
    }
  }

  this->data.swap(nextData);
  dataParts.swap(nextDataParts);
}

std::vector<uint8_t> * Node::serialize() const
{
  return new std::vector<uint8_t>(serializeValue());
}

std::vector<uint8_t> * Node::serialize(const SerializationLimits& limits) const
{
  return new std::vector<uint8_t>(serializeValue(limits));
}

std::vector<uint8_t> Node::serializeValue() const
{
  return serializeValue(SerializationLimits());
}

std::vector<uint8_t> Node::serializeValue(const SerializationLimits& limits) const
{
  std::vector<uint8_t> output;
  output.reserve(18);

  SerialState state(limits);
  serializeNode(*this, output, state, 0, false);
  return output;
}

std::string Node::to_template_string(const std::string& start, const std::string& stop) const
{
  return to_template_string(start, stop, TemplateStringLimits());
}

std::string Node::to_template_string(
    const std::string& start, const std::string& stop, const TemplateStringLimits& limits) const
{
  std::string output;
  TemplateStringState state(limits);
  appendNodeTemplate(*this, start, stop, output, state, 0);
  return output;
}

std::unique_ptr<Node> Node::unserializeOwned(std::string_view serial)
{
  return unserializeOwned(serial, SerializationLimits());
}

std::unique_ptr<Node> Node::unserializeOwned(std::string_view serial, const SerializationLimits& limits)
{
  return unserializeOwnedRange(reinterpret_cast<const uint8_t *>(serial.data()), serial.size(), 0, limits);
}

Node * Node::unserialize(std::vector<uint8_t>& serial, size_t offset, size_t * vpos)
{
  return unserialize(serial.data(), serial.size(), offset, vpos);
}

Node * Node::unserialize(std::vector<uint8_t>& serial, size_t offset, size_t * vpos, const SerializationLimits& limits)
{
  return unserialize(serial.data(), serial.size(), offset, vpos, limits);
}

Node * Node::unserialize(const uint8_t * serial, size_t length, size_t offset, size_t * vpos)
{
  const SerializationLimits limits;
  return unserialize(serial, length, offset, vpos, limits);
}

Node * Node::unserialize(
    const uint8_t * serial, size_t length, size_t offset, size_t * vpos, const SerializationLimits& limits)
{
  if (vpos == NULL) {
    throw Exception("Missing serial output position");
  }
  std::unique_ptr<Node> node(unserializeOwnedRange(serial, length, offset, limits));
  *vpos = length;
  return node.release();
}

Node * Node::unserialize(std::string_view serial, size_t offset, size_t * vpos)
{
  return unserialize(reinterpret_cast<const uint8_t *>(serial.data()), serial.size(), offset, vpos);
}

Node * Node::unserialize(std::string_view serial, size_t offset, size_t * vpos, const SerializationLimits& limits)
{
  return unserialize(reinterpret_cast<const uint8_t *>(serial.data()), serial.size(), offset, vpos, limits);
}

} // namespace mustache
