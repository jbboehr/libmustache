#include "cista-archive.hpp"

#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3) && defined(CISTA_FNV1A)
#undef CISTA_FNV1A
#endif
#include <cista/serialization.h>
#include <xxhash.h>
#include <zlib.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mustache_benchmark {

namespace {

namespace archive_data = cista::offset;

constexpr std::uint64_t archiveMagic = UINT64_C(0x4D55535443495354);
constexpr std::uint32_t archiveVersion = 1;
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
constexpr cista::mode archiveVersionMode = cista::mode::WITH_VERSION;
#else
constexpr cista::mode archiveVersionMode = cista::mode::WITH_STATIC_VERSION;
#endif
constexpr cista::mode archiveModeNeither = archiveVersionMode;
constexpr cista::mode archiveModeDeepCheck = archiveVersionMode | cista::mode::DEEP_CHECK;
constexpr cista::mode archiveModeIntegrity = archiveVersionMode | cista::mode::WITH_INTEGRITY;
constexpr cista::mode archiveModeDeepCheckAndIntegrity =
    archiveVersionMode | cista::mode::WITH_INTEGRITY | cista::mode::DEEP_CHECK;
constexpr std::size_t renderNestingCeiling = 256;
constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

enum Presence : std::uint8_t {
  HasData = 1,
  HasStartSequence = 2,
  HasStopSequence = 4,
};

struct ArchiveSlice {
    std::uint32_t offset = 0;
    std::uint32_t length = 0;
};

struct ArchiveNode {
    ArchiveSlice data;
    ArchiveSlice startSequence;
    ArchiveSlice stopSequence;
    std::uint32_t firstChild = invalidIndex;
    std::uint32_t nextSibling = invalidIndex;
    std::uint16_t type = 0;
    std::uint16_t flags = 0;
    std::uint8_t presence = 0;
    std::uint8_t reserved0 = 0;
    std::uint8_t reserved1 = 0;
    std::uint8_t reserved2 = 0;
};

struct ArchivePartial {
    ArchiveSlice name;
    std::uint32_t root = 0;
};

struct ArchiveGraph {
    std::uint64_t magic = archiveMagic;
    std::uint32_t version = archiveVersion;
    std::uint32_t root = 0;
    std::uint64_t serializedSize = 0;
    archive_data::vector<ArchiveNode> nodes;
    archive_data::vector<ArchivePartial> partials;
    archive_data::vector<std::uint8_t> strings;
};

std::string_view archiveSliceView(const ArchiveGraph& graph, const ArchiveSlice& value)
{
  if (value.length == 0) {
    return std::string_view();
  }
  return std::string_view(reinterpret_cast<const char *>(graph.strings.data() + value.offset), value.length);
}

using NodeTypeValue = std::underlying_type_t<mustache::Node::Type>;

NodeTypeValue nodeTypeValue(const mustache::Node& node) noexcept
{
  NodeTypeValue value;
  static_assert(sizeof(value) == sizeof(node.type), "Node type representation changed");
  std::memcpy(&value, &node.type, sizeof(value));
  return value;
}

bool isSerializableType(NodeTypeValue type)
{
  switch (type) {
    case static_cast<NodeTypeValue>(mustache::Node::TypeRoot):
    case static_cast<NodeTypeValue>(mustache::Node::TypeOutput):
    case static_cast<NodeTypeValue>(mustache::Node::TypeTag):
    case static_cast<NodeTypeValue>(mustache::Node::TypeVariable):
    case static_cast<NodeTypeValue>(mustache::Node::TypeNegate):
    case static_cast<NodeTypeValue>(mustache::Node::TypeSection):
    case static_cast<NodeTypeValue>(mustache::Node::TypeStop):
    case static_cast<NodeTypeValue>(mustache::Node::TypeComment):
    case static_cast<NodeTypeValue>(mustache::Node::TypePartial):
    case static_cast<NodeTypeValue>(mustache::Node::TypeInlinePartial):
      return true;
    default:
      return false;
  }
}

bool typeAllowsChildren(mustache::Node::Type type)
{
  return (type & mustache::Node::TypeHasChildren) != 0;
}

bool typeUsesDataParts(mustache::Node::Type type)
{
  return (type & mustache::Node::TypeHasDot) != 0;
}

void addBounded(std::size_t amount, std::size_t maximum, std::size_t * total, const char * message)
{
  if (*total > maximum || amount > maximum - *total) {
    throw mustache::Exception(message);
  }
  *total += amount;
}

class ArchiveBuilder {
  public:
    ArchiveGraph build(const mustache::Node& root, const mustache::Node::Partials& partials)
    {
      countNode(root, 0);
      for (const mustache::Node::PartialPair& partial : partials) {
        if (partial.second == nullptr) {
          throw mustache::Exception("Invalid null Cista archive partial");
        }
        addStringBytes(partial.first.size());
        countNode(*partial.second, 0);
      }
      if (nodeCount_ > std::numeric_limits<std::uint32_t>::max() ||
          stringBytes_ > std::numeric_limits<std::uint32_t>::max() ||
          partials.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw mustache::Exception("Cista archive exceeds format limit");
      }
      graph_.nodes.reserve(static_cast<std::uint32_t>(nodeCount_));
      graph_.partials.reserve(static_cast<std::uint32_t>(partials.size()));
      graph_.strings.reserve(static_cast<std::uint32_t>(stringBytes_));

      graph_.root = appendNode(root);
      for (const mustache::Node::PartialPair& partial : partials) {
        ArchivePartial archived{};
        archived.name = appendString(partial.first);
        archived.root = appendNode(*partial.second);
        graph_.partials.push_back(std::move(archived));
      }
      return std::move(graph_);
    }

  private:
    void addStringBytes(std::size_t bytes)
    {
      if (stringBytes_ > std::numeric_limits<std::uint32_t>::max() ||
          bytes > std::numeric_limits<std::uint32_t>::max() - stringBytes_) {
        throw mustache::Exception("Cista archive string bytes exceed format limit");
      }
      stringBytes_ += bytes;
    }

    void countNode(const mustache::Node& source, std::size_t depth)
    {
      if (depth >= renderNestingCeiling || nodeCount_ == std::numeric_limits<std::uint32_t>::max()) {
        throw mustache::Exception("Cista archive node limit exceeded");
      }
      ++nodeCount_;
      if (source.data.has_value()) {
        addStringBytes(source.data->size());
      }
      if (source.startSequence.has_value()) {
        addStringBytes(source.startSequence->size());
      }
      if (source.stopSequence.has_value()) {
        addStringBytes(source.stopSequence->size());
      }
      for (const std::unique_ptr<mustache::Node>& child : source.children) {
        if (child == nullptr) {
          throw mustache::Exception("Invalid null Cista archive child");
        }
        countNode(*child, depth + 1);
      }
    }

    ArchiveSlice appendString(std::string_view value)
    {
      ArchiveSlice result;
      result.offset = static_cast<std::uint32_t>(graph_.strings.size());
      result.length = static_cast<std::uint32_t>(value.size());
      if (!value.empty()) {
        graph_.strings.insert(graph_.strings.end(), reinterpret_cast<const std::uint8_t *>(value.data()),
            reinterpret_cast<const std::uint8_t *>(value.data()) + value.size());
      }
      return result;
    }

    std::uint32_t appendNode(const mustache::Node& source)
    {
      if (graph_.nodes.size() == std::numeric_limits<std::uint32_t>::max()) {
        throw mustache::Exception("Cista archive node count exceeds format limit");
      }
      if (!source.partials.empty()) {
        throw mustache::Exception("Cista archive experiment does not support inline partial ownership");
      }
      if (source.child != nullptr) {
        throw mustache::Exception("Cista archive experiment does not support container nodes");
      }
      if (source.flags < 0 || static_cast<unsigned int>(source.flags) > std::numeric_limits<std::uint16_t>::max()) {
        throw mustache::Exception("Cista archive node flags exceed format limit");
      }
      const NodeTypeValue type = nodeTypeValue(source);
      if (!isSerializableType(type)) {
        throw mustache::Exception("Invalid Cista archive node type");
      }

      const std::uint32_t index = static_cast<std::uint32_t>(graph_.nodes.size());
      graph_.nodes.emplace_back();

      ArchiveNode archived{};
      archived.type = static_cast<std::uint16_t>(type);
      archived.flags = static_cast<std::uint16_t>(source.flags);
      if (source.data.has_value()) {
        archived.presence |= HasData;
        archived.data = appendString(*source.data);
      }
      if (source.startSequence.has_value()) {
        archived.presence |= HasStartSequence;
        archived.startSequence = appendString(*source.startSequence);
      }
      if (source.stopSequence.has_value()) {
        archived.presence |= HasStopSequence;
        archived.stopSequence = appendString(*source.stopSequence);
      }
      std::uint32_t previousChild = invalidIndex;
      for (const std::unique_ptr<mustache::Node>& child : source.children) {
        const std::uint32_t childIndex = appendNode(*child);
        if (archived.firstChild == invalidIndex) {
          archived.firstChild = childIndex;
        } else {
          graph_.nodes[previousChild].nextSibling = childIndex;
        }
        previousChild = childIndex;
      }
      graph_.nodes[index] = std::move(archived);
      return index;
    }

    ArchiveGraph graph_{};
    std::size_t nodeCount_ = 0;
    std::size_t stringBytes_ = 0;
};

class ArchiveValidator {
  public:
    ArchiveValidator(const ArchiveGraph& graph, const CistaArchiveLimits& limits, std::size_t inputBytes = 0) :
        graph_(graph),
        limits_(limits),
        inputBytes_(inputBytes),
        nodes_(0),
        dataParts_(0)
    {}

    void validate()
    {
      if (graph_.magic != archiveMagic || graph_.version != archiveVersion || graph_.nodes.empty()) {
        throw mustache::Exception("Invalid Cista archive header");
      }
      if (graph_.nodes.size() > limits_.maxNodes) {
        throw mustache::Exception("Cista archive node count limit exceeded");
      }
      states_.assign(graph_.nodes.size(), 0);
      if (inputBytes_ != 0 && graph_.serializedSize != inputBytes_) {
        throw mustache::Exception("Invalid Cista archive encoded length");
      }
      if (graph_.strings.size() > limits_.maxStringBytes) {
        throw mustache::Exception("Cista archive string byte limit exceeded");
      }
      validateRoot(graph_.root);
      std::string_view previousName;
      bool hasPreviousName = false;
      for (const ArchivePartial& partial : graph_.partials) {
        const std::string_view name = validateString(partial.name);
        if (name.empty()) {
          throw mustache::Exception("Invalid empty Cista archive partial name");
        }
        if (hasPreviousName && !(previousName < name)) {
          throw mustache::Exception("Cista archive partial names are not canonical");
        }
        previousName = name;
        hasPreviousName = true;
        validateRoot(partial.root);
      }
      if (nodes_ != graph_.nodes.size()) {
        throw mustache::Exception("Unreachable Cista archive node");
      }
    }

  private:
    std::string_view validateString(const ArchiveSlice& value) const
    {
      if (value.offset > graph_.strings.size() || value.length > graph_.strings.size() - value.offset) {
        throw mustache::Exception("Invalid Cista archive string range");
      }
      return archiveSliceView(graph_, value);
    }

    void validateRoot(std::uint32_t index)
    {
      if (index >= graph_.nodes.size() || graph_.nodes[index].type != mustache::Node::TypeRoot) {
        throw mustache::Exception("Invalid Cista archive root");
      }
      if (graph_.nodes[index].nextSibling != invalidIndex) {
        throw mustache::Exception("Invalid Cista archive root sibling");
      }
      validateNode(index, 0, false);
    }

    void validateNode(std::uint32_t index, std::size_t depth, bool partialIndentationMetadata)
    {
      if (index >= graph_.nodes.size()) {
        throw mustache::Exception("Invalid Cista archive node index");
      }
      if (depth >= limits_.maxNestingDepth || depth >= renderNestingCeiling) {
        throw mustache::Exception("Cista archive nesting limit exceeded");
      }
      if (nodes_ >= limits_.maxNodes) {
        throw mustache::Exception("Cista archive node count limit exceeded");
      }
      if (states_[index] != 0) {
        throw mustache::Exception("Cista archive nodes must form disjoint trees");
      }
      states_[index] = 1;
      ++nodes_;

      const ArchiveNode& node = graph_.nodes[index];
      if (!isSerializableType(static_cast<NodeTypeValue>(node.type))) {
        throw mustache::Exception("Invalid Cista archive node type");
      }
      const mustache::Node::Type type = static_cast<mustache::Node::Type>(node.type);
      const std::uint16_t validFlags = static_cast<std::uint16_t>(
          mustache::Node::FlagEscape | mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent);
      if ((node.flags & ~validFlags) != 0 ||
          ((node.flags & mustache::Node::FlagLambdaOnly) != 0 &&
              (type != mustache::Node::TypeOutput ||
                  (node.flags != mustache::Node::FlagLambdaOnly &&
                      node.flags != (mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent)))) ||
          ((node.flags & mustache::Node::FlagPartialIndent) != 0 &&
              (type != mustache::Node::TypeOutput ||
                  node.flags != (mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent)))) {
        throw mustache::Exception("Invalid Cista archive node flags");
      }
      if ((node.flags & mustache::Node::FlagPartialIndent) != 0 && !partialIndentationMetadata) {
        throw mustache::Exception("Invalid Cista archive partial indentation metadata");
      }
      if ((node.presence & ~(HasData | HasStartSequence | HasStopSequence)) != 0) {
        throw mustache::Exception("Invalid Cista archive node presence bits");
      }
      if (node.reserved0 != 0 || node.reserved1 != 0 || node.reserved2 != 0) {
        throw mustache::Exception("Invalid Cista archive reserved bytes");
      }

      const bool hasData = (node.presence & HasData) != 0;
      if ((type == mustache::Node::TypeRoot && hasData) || (type != mustache::Node::TypeRoot && !hasData)) {
        throw mustache::Exception("Invalid Cista archive node data");
      }
      const std::string_view data = validateString(node.data);
      if (!hasData && (node.data.offset != 0 || node.data.length != 0)) {
        throw mustache::Exception("Invalid absent Cista archive node data");
      }
      if (node.firstChild != invalidIndex && !typeAllowsChildren(type)) {
        throw mustache::Exception("Invalid Cista archive node children");
      }

      const bool hasStart = (node.presence & HasStartSequence) != 0;
      const bool hasStop = (node.presence & HasStopSequence) != 0;
      validateString(node.startSequence);
      validateString(node.stopSequence);
      if (hasStart != hasStop || ((hasStart || hasStop) && type != mustache::Node::TypeSection) ||
          (!hasStart &&
              (node.startSequence.offset != 0 || node.startSequence.length != 0 || node.stopSequence.offset != 0 ||
                  node.stopSequence.length != 0))) {
        throw mustache::Exception("Invalid Cista archive section delimiters");
      }

      validateDataParts(node, type, data);
      validateChildren(node, depth);
      states_[index] = 2;
    }

    void validateDataParts(const ArchiveNode&, mustache::Node::Type type, std::string_view data)
    {
      if (!typeUsesDataParts(type)) {
        return;
      }
      const std::size_t dots = static_cast<std::size_t>(std::count(data.begin(), data.end(), '.'));
      const std::size_t parts = dots + 1;
      if (parts > limits_.maxDataPartsPerNode) {
        throw mustache::Exception("Cista archive per-node data-part limit exceeded");
      }
      addBounded(parts, limits_.maxDataParts, &dataParts_, "Cista archive data-part limit exceeded");
    }

    void validateChildren(const ArchiveNode& node, std::size_t depth)
    {
      std::uint32_t childIndex = node.firstChild;
      while (childIndex != invalidIndex) {
        if (childIndex >= graph_.nodes.size()) {
          throw mustache::Exception("Invalid Cista archive child index");
        }
        const ArchiveNode& child = graph_.nodes[childIndex];
        const std::uint32_t nextSibling = child.nextSibling;
        if (nextSibling != invalidIndex && nextSibling >= graph_.nodes.size()) {
          throw mustache::Exception("Invalid Cista archive sibling index");
        }
        bool indentationMetadata = false;
        if ((child.flags & mustache::Node::FlagPartialIndent) != 0) {
          if (child.type != mustache::Node::TypeOutput ||
              child.flags != (mustache::Node::FlagLambdaOnly | mustache::Node::FlagPartialIndent) ||
              (child.presence & HasData) == 0 || nextSibling == invalidIndex) {
            throw mustache::Exception("Invalid Cista archive partial indentation metadata");
          }
          const std::string_view indentation = validateString(child.data);
          if (!std::all_of(indentation.begin(), indentation.end(),
                  [](char value) {
                    return value == ' ' || value == '\t';
                  }) ||
              graph_.nodes[nextSibling].type != mustache::Node::TypePartial) {
            throw mustache::Exception("Invalid Cista archive partial indentation metadata");
          }
          indentationMetadata = true;
        }
        validateNode(childIndex, depth + 1, indentationMetadata);
        childIndex = nextSibling;
      }
    }

    const ArchiveGraph& graph_;
    const CistaArchiveLimits& limits_;
    std::size_t inputBytes_;
    std::vector<std::uint8_t> states_;
    std::size_t nodes_;
    std::size_t dataParts_;
};

class ArchiveRenderer {
  public:
    ArchiveRenderer(const ArchiveGraph& graph, const mustache::Data& data, const mustache::RenderLimits& limits) :
        graph_(graph),
        data_(data),
        limits_(limits),
        outputBytes_(0),
        nodeVisits_(0)
    {}

    std::string render()
    {
      output_.reserve(std::min<std::size_t>(mustache::Renderer::outputBufferLength, limits_.maxOutputBytes));
      stack_.push_back(&data_);
      renderNode(graph_.root, 0, nullptr, false);
      return std::move(output_);
    }

  private:
    struct IndentationFrame {
        std::vector<std::string_view> components;
        bool atLineStart = true;
    };

    const mustache::Data * findInMap(const mustache::Data& data, std::string_view key) const
    {
      if (data.type() != mustache::Data::TypeMap) {
        return nullptr;
      }
      return data.find(std::string(key));
    }

    const mustache::Data * lookup(const ArchiveNode& node)
    {
      const mustache::Data * data = stack_.back();
      const std::string_view name = archiveSliceView(graph_, node.data);
      if (name == ".") {
        return data;
      }
      if (const mustache::Data * found = findInMap(*data, name)) {
        return found;
      }

      const std::size_t firstDelimiter = name.find('.');
      const std::string_view initial = name.substr(0, firstDelimiter);
      const mustache::Data * reference = nullptr;
      for (std::vector<const mustache::Data *>::const_reverse_iterator position = stack_.rbegin();
          position != stack_.rend(); ++position) {
        if (*position != nullptr && (reference = findInMap(**position, initial)) != nullptr) {
          break;
        }
      }
      std::size_t offset = firstDelimiter;
      while (reference != nullptr && offset != std::string_view::npos) {
        const std::size_t start = offset + 1;
        offset = name.find('.', start);
        reference = findInMap(*reference, name.substr(start, offset - start));
      }
      return reference;
    }

    void consumeNodeVisit(std::size_t depth)
    {
      if (depth >= limits_.maxNestingDepth || depth >= renderNestingCeiling) {
        throw mustache::Exception("Render nesting limit exceeded");
      }
      if (nodeVisits_ >= limits_.maxNodeVisits) {
        throw mustache::Exception("Render node visit limit exceeded");
      }
      ++nodeVisits_;
    }

    void append(std::string_view value)
    {
      if (outputBytes_ > limits_.maxOutputBytes || value.size() > limits_.maxOutputBytes - outputBytes_ ||
          output_.size() > output_.max_size() || value.size() > output_.max_size() - output_.size()) {
        throw mustache::Exception("Render output byte limit exceeded");
      }
      outputBytes_ += value.size();
      if (!value.empty()) {
        output_.append(value.data(), value.size());
      }
    }

    void appendEscaped(std::string_view value)
    {
      for (const char character : value) {
        switch (character) {
          case '&':
            append("&amp;");
            break;
          case '"':
            append("&quot;");
            break;
          case '\'':
            append("&#039;");
            break;
          case '<':
            append("&lt;");
            break;
          case '>':
            append("&gt;");
            break;
          default:
            append(std::string_view(&character, 1));
            break;
        }
      }
    }

    void appendIndentation(const IndentationFrame& frame)
    {
      for (const std::string_view component : frame.components) {
        append(component);
      }
    }

    void appendTemplateOutput(std::string_view value)
    {
      if (indentationStack_.empty()) {
        append(value);
        return;
      }
      IndentationFrame& frame = indentationStack_.back();
      std::size_t offset = 0;
      while (offset < value.size()) {
        if (frame.atLineStart) {
          appendIndentation(frame);
          frame.atLineStart = false;
        }
        const std::size_t newline = value.find('\n', offset);
        if (newline == std::string_view::npos) {
          append(value.substr(offset));
          return;
        }
        append(value.substr(offset, newline - offset + 1));
        frame.atLineStart = true;
        offset = newline + 1;
      }
    }

    void consumeTemplateSource(std::string_view value)
    {
      if (indentationStack_.empty()) {
        return;
      }
      IndentationFrame& frame = indentationStack_.back();
      if (value.empty()) {
        frame.atLineStart = false;
        return;
      }
      for (const char character : value) {
        frame.atLineStart = character == '\n';
      }
    }

    void beginTemplateTag()
    {
      if (!indentationStack_.empty() && indentationStack_.back().atLineStart) {
        appendIndentation(indentationStack_.back());
        indentationStack_.back().atLineStart = false;
      }
    }

    void renderChildren(const ArchiveNode& node, std::size_t depth)
    {
      std::uint32_t childIndex = node.firstChild;
      while (childIndex != invalidIndex) {
        const ArchiveNode& child = graph_.nodes[childIndex];
        const std::uint32_t nextSibling = child.nextSibling;
        if ((child.flags & mustache::Node::FlagPartialIndent) != 0) {
          renderNode(childIndex, depth + 1, nullptr, true);
          const std::string_view indentation = archiveSliceView(graph_, child.data);
          renderNode(nextSibling, depth + 1, &indentation, false);
          childIndex = graph_.nodes[nextSibling].nextSibling;
        } else {
          renderNode(childIndex, depth + 1, nullptr, false);
          childIndex = nextSibling;
        }
      }
    }

    void renderWithContext(const ArchiveNode& node, const mustache::Data& context, std::size_t depth)
    {
      stack_.push_back(&context);
      try {
        renderChildren(node, depth);
      } catch (...) {
        stack_.pop_back();
        throw;
      }
      stack_.pop_back();
    }

    const ArchivePartial * findPartial(std::string_view name) const
    {
      const auto position = std::lower_bound(graph_.partials.begin(), graph_.partials.end(), name,
          [this](const ArchivePartial& partial, std::string_view expected) {
            return archiveSliceView(graph_, partial.name) < expected;
          });
      if (position != graph_.partials.end() && archiveSliceView(graph_, position->name) == name) {
        return &*position;
      }
      return nullptr;
    }

    void renderNode(std::uint32_t index, std::size_t depth, const std::string_view * partialIndentation,
        bool partialIndentationMetadata)
    {
      consumeNodeVisit(depth);
      const ArchiveNode& node = graph_.nodes[index];
      if ((node.flags & mustache::Node::FlagPartialIndent) != 0 && !partialIndentationMetadata) {
        throw mustache::Exception("Invalid Cista archive partial indentation metadata");
      }

      const mustache::Node::Type type = static_cast<mustache::Node::Type>(node.type);
      const mustache::Data * value = nullptr;
      bool valueIsEmpty = true;
      if ((type & mustache::Node::TypeHasData) != 0) {
        value = lookup(node);
        valueIsEmpty = value == nullptr || value->isEmpty();
      }

      switch (type) {
        case mustache::Node::TypeNone:
          return;
        case mustache::Node::TypeComment:
        case mustache::Node::TypeStop:
        case mustache::Node::TypeInlinePartial:
          beginTemplateTag();
          return;
        case mustache::Node::TypeRoot:
          renderChildren(node, depth);
          return;
        case mustache::Node::TypeOutput:
          if ((node.presence & HasData) != 0) {
            if ((node.flags & mustache::Node::FlagLambdaOnly) != 0) {
              consumeTemplateSource(archiveSliceView(graph_, node.data));
            } else {
              appendTemplateOutput(archiveSliceView(graph_, node.data));
            }
          }
          return;
        case mustache::Node::TypeTag:
        case mustache::Node::TypeVariable:
          beginTemplateTag();
          if (!valueIsEmpty) {
            renderValue(node, *value);
          }
          return;
        case mustache::Node::TypeNegate:
          beginTemplateTag();
          if (valueIsEmpty) {
            renderChildren(node, depth);
          }
          return;
        case mustache::Node::TypeSection:
          beginTemplateTag();
          if (!valueIsEmpty) {
            renderSection(node, *value, depth);
          }
          return;
        case mustache::Node::TypePartial: {
          beginTemplateTag();
          const ArchivePartial * partial = findPartial(archiveSliceView(graph_, node.data));
          if (partial == nullptr) {
            return;
          }
          IndentationFrame frame;
          if (partialIndentation != nullptr) {
            if (!indentationStack_.empty()) {
              frame.components = indentationStack_.back().components;
            }
            if (!partialIndentation->empty()) {
              frame.components.push_back(*partialIndentation);
            }
          }
          indentationStack_.push_back(std::move(frame));
          try {
            renderNode(partial->root, depth + 1, nullptr, false);
          } catch (...) {
            indentationStack_.pop_back();
            throw;
          }
          indentationStack_.pop_back();
          return;
        }
        default:
          throw mustache::Exception("Unsupported Cista archive node type");
      }
    }

    void renderValue(const ArchiveNode& node, const mustache::Data& value)
    {
      std::string rendered;
      std::string_view view;
      switch (value.type()) {
        case mustache::Data::TypeString:
          view = value.stringValue();
          break;
        case mustache::Data::TypeBoolean:
        case mustache::Data::TypeInteger:
        case mustache::Data::TypeDouble:
          rendered = value.toString();
          view = rendered;
          break;
        case mustache::Data::TypeLambda:
          throw mustache::Exception("Cista archive experiment does not support lambdas");
        case mustache::Data::TypeNone:
        case mustache::Data::TypeList:
        case mustache::Data::TypeMap:
        case mustache::Data::TypeArray:
          return;
      }
      if ((node.flags & mustache::Node::FlagEscape) != 0) {
        appendEscaped(view);
      } else {
        append(view);
      }
    }

    void renderSection(const ArchiveNode& node, const mustache::Data& value, std::size_t depth)
    {
      switch (value.type()) {
        case mustache::Data::TypeString:
        case mustache::Data::TypeBoolean:
        case mustache::Data::TypeInteger:
        case mustache::Data::TypeDouble:
        case mustache::Data::TypeMap:
          renderWithContext(node, value, depth);
          return;
        case mustache::Data::TypeList:
          for (const mustache::Data& child : value.listItems()) {
            renderWithContext(node, child, depth);
          }
          return;
        case mustache::Data::TypeArray:
          for (const mustache::Data& child : value.arrayItems()) {
            renderWithContext(node, child, depth);
          }
          return;
        case mustache::Data::TypeLambda:
          throw mustache::Exception("Cista archive experiment does not support lambdas");
        case mustache::Data::TypeNone:
          return;
      }
    }

    const ArchiveGraph& graph_;
    const mustache::Data& data_;
    const mustache::RenderLimits& limits_;
    std::string output_;
    std::vector<const mustache::Data *> stack_;
    std::vector<IndentationFrame> indentationStack_;
    std::size_t outputBytes_;
    std::size_t nodeVisits_;
};

template <cista::mode Mode> const ArchiveGraph& readArchive(std::string_view bytes, const CistaArchiveLimits& limits)
{
  if (bytes.empty()) {
    throw mustache::Exception("Empty Cista archive");
  }
  if (bytes.size() > limits.maxInputBytes) {
    throw mustache::Exception("Cista archive input byte limit exceeded");
  }
  if (reinterpret_cast<std::uintptr_t>(bytes.data()) % alignof(ArchiveGraph) != 0) {
    throw mustache::Exception("Unaligned Cista archive buffer");
  }
  const ArchiveGraph * graph = cista::deserialize<ArchiveGraph, Mode>(bytes);
  if (graph == nullptr) {
    throw mustache::Exception("Invalid Cista archive root");
  }
  ArchiveValidator(*graph, limits, bytes.size()).validate();
  return *graph;
}

template <cista::mode Mode>
std::vector<std::uint8_t> serializeCistaArchiveWithMode(
    const mustache::Node& root, const mustache::Node::Partials& partials, const CistaArchiveLimits& archiveLimits)
{
  ArchiveGraph graph = ArchiveBuilder().build(root, partials);
  ArchiveValidator(graph, archiveLimits).validate();
  std::vector<std::uint8_t> bytes = cista::serialize<Mode>(graph);
  graph.serializedSize = bytes.size();
  bytes = cista::serialize<Mode>(graph);
  if (bytes.size() != graph.serializedSize) {
    throw mustache::Exception("Cista archive size changed while framing");
  }
  if (bytes.size() > archiveLimits.maxInputBytes) {
    throw mustache::Exception("Cista archive output byte limit exceeded");
  }
  return bytes;
}

template <cista::mode Mode>
std::string renderCistaArchiveWithMode(std::string_view bytes, const mustache::Data& data,
    const CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits)
{
  return ArchiveRenderer(readArchive<Mode>(bytes, archiveLimits), data, renderLimits).render();
}

} // namespace

const char * cistaSecurityModeName(CistaSecurityMode mode) noexcept
{
  switch (mode) {
    case CistaSecurityMode::Neither:
      return "neither";
    case CistaSecurityMode::DeepCheck:
      return "deep_check";
    case CistaSecurityMode::Integrity:
      return "integrity";
    case CistaSecurityMode::DeepCheckAndIntegrity:
      return "deep_check_integrity";
  }
  return "unknown";
}

const char * cistaVersionModeName() noexcept
{
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
  return "version";
#else
  return "static_version";
#endif
}

const char * cistaIntegrityAlgorithmName() noexcept
{
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
  return "cista_xxh3_64";
#else
  return "cista_fnv1a_64";
#endif
}

const char * cistaChecksumAlgorithmName(CistaChecksumAlgorithm algorithm) noexcept
{
  switch (algorithm) {
    case CistaChecksumAlgorithm::None:
      return "none";
    case CistaChecksumAlgorithm::Fnv1a64:
      return "fnv1a_64";
    case CistaChecksumAlgorithm::Crc32:
      return "crc32";
    case CistaChecksumAlgorithm::Xxh3_64:
      return "xxh3_64";
  }
  return "unknown";
}

std::uint64_t checksumCistaArchive(std::string_view bytes, CistaChecksumAlgorithm algorithm)
{
  const char * data = bytes.empty() ? "" : bytes.data();
  switch (algorithm) {
    case CistaChecksumAlgorithm::None:
      return 0;
    case CistaChecksumAlgorithm::Fnv1a64: {
      std::uint64_t hash = UINT64_C(14695981039346656037);
      for (const unsigned char byte : bytes) {
        hash = (hash ^ byte) * UINT64_C(1099511628211);
      }
      return hash;
    }
    case CistaChecksumAlgorithm::Crc32:
      return static_cast<std::uint32_t>(
          crc32_z(0, reinterpret_cast<const Bytef *>(data), static_cast<z_size_t>(bytes.size())));
    case CistaChecksumAlgorithm::Xxh3_64:
#if defined(MUSTACHE_CISTA_RUNTIME_VERSION_XXH3)
      return cista::XXH3_64bits(data, bytes.size());
#else
      return XXH3_64bits(data, bytes.size());
#endif
  }
  throw mustache::Exception("Unknown Cista checksum algorithm");
}

std::vector<std::uint8_t> serializeCistaArchive(
    const mustache::Node& root, const mustache::Node::Partials& partials, const CistaArchiveLimits& archiveLimits)
{
  return serializeCistaArchive(root, partials, CistaSecurityMode::DeepCheckAndIntegrity, archiveLimits);
}

std::vector<std::uint8_t> serializeCistaArchive(const mustache::Node& root, const mustache::Node::Partials& partials,
    CistaSecurityMode mode, const CistaArchiveLimits& archiveLimits)
{
  switch (mode) {
    case CistaSecurityMode::Neither:
      return serializeCistaArchiveWithMode<archiveModeNeither>(root, partials, archiveLimits);
    case CistaSecurityMode::DeepCheck:
      return serializeCistaArchiveWithMode<archiveModeDeepCheck>(root, partials, archiveLimits);
    case CistaSecurityMode::Integrity:
      return serializeCistaArchiveWithMode<archiveModeIntegrity>(root, partials, archiveLimits);
    case CistaSecurityMode::DeepCheckAndIntegrity:
      return serializeCistaArchiveWithMode<archiveModeDeepCheckAndIntegrity>(root, partials, archiveLimits);
  }
  throw mustache::Exception("Unknown Cista security mode");
}

std::string renderCistaArchive(std::string_view bytes, const mustache::Data& data,
    const CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits)
{
  return renderCistaArchive(bytes, data, CistaSecurityMode::DeepCheckAndIntegrity, archiveLimits, renderLimits);
}

std::string renderCistaArchive(std::string_view bytes, const mustache::Data& data, CistaSecurityMode mode,
    const CistaArchiveLimits& archiveLimits, const mustache::RenderLimits& renderLimits)
{
  switch (mode) {
    case CistaSecurityMode::Neither:
      return renderCistaArchiveWithMode<archiveModeNeither>(bytes, data, archiveLimits, renderLimits);
    case CistaSecurityMode::DeepCheck:
      return renderCistaArchiveWithMode<archiveModeDeepCheck>(bytes, data, archiveLimits, renderLimits);
    case CistaSecurityMode::Integrity:
      return renderCistaArchiveWithMode<archiveModeIntegrity>(bytes, data, archiveLimits, renderLimits);
    case CistaSecurityMode::DeepCheckAndIntegrity:
      return renderCistaArchiveWithMode<archiveModeDeepCheckAndIntegrity>(bytes, data, archiveLimits, renderLimits);
  }
  throw mustache::Exception("Unknown Cista security mode");
}

} // namespace mustache_benchmark
