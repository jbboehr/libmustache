#include "cli.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <fstream>
#include <limits>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "mustache_config.h"
#include "mustache.hpp"

namespace mustache_cli {
namespace {

constexpr std::size_t maxRepetitions = 1000000;

enum class Action {
  Render,
  Help,
  Version
};

enum class DataFormat {
  Json,
  Yaml
};

struct Options {
    Action action = Action::Render;
    std::string templatePath;
    std::string dataPath;
    std::string outputPath;
    std::map<std::string, std::string> partialPaths;
    std::size_t repetitions = 1;
    bool repetitionsSet = false;
    bool deprecatedReadable = false;
};

void showUsage(std::ostream& output)
{
  output << "Usage: mustachec -t <template> [-d <data>] [options]\n"
         << '\n'
         << "Render a Mustache template using JSON or YAML data.\n"
         << '\n'
         << "Options:\n"
         << "  -t, --template <file>     Template file (required)\n"
         << "  -d, --data <file>         JSON or YAML data (default: null)\n"
         << "  -l, --partial <name=file> Add a named partial; may be repeated\n"
         << "  -o, --output <file>       Write output to a file instead of stdout\n"
         << "  -n, --repeat <count>      Render 1-" << maxRepetitions << " times (default: 1)\n"
         << "  -r                         Deprecated ignored compatibility option\n"
         << "  -h, --help                Show this help\n"
         << "  -v, --version             Show version and feature information\n";
}

void showVersion(std::ostream& output)
{
  output << "mustachec " << mustache_version() << '\n';
#ifdef MUSTACHE_HAVE_LIBJSON
  output << "JSON support: nlohmann/json\n";
#else
  output << "JSON support: none\n";
#endif
#ifdef MUSTACHE_HAVE_LIBYAML
  output << "YAML support: libyaml\n";
#else
  output << "YAML support: none\n";
#endif
  output << "C++ standard: C++" << MUSTACHE_CXX_STANDARD << '\n';
}

bool readOptionValue(const std::vector<std::string>& arguments, std::size_t& index, std::string_view shortOption,
    std::string_view longOption, std::string& value)
{
  const std::string& argument = arguments[index];
  if (argument == shortOption || argument == longOption) {
    if (index + 1 >= arguments.size()) {
      throw std::runtime_error("Option " + argument + " requires an argument");
    }
    value = arguments[++index];
  } else if (argument.size() > shortOption.size() && argument.compare(0, shortOption.size(), shortOption) == 0) {
    value = argument.substr(shortOption.size());
    if (value.front() == '=') {
      throw std::runtime_error("Option " + std::string(shortOption) + " does not use '='; separate its value or use " +
          std::string(longOption) + "=<value>");
    }
  } else {
    const std::string prefix = std::string(longOption) + "=";
    if (argument.compare(0, prefix.size(), prefix) != 0) {
      return false;
    }
    value = argument.substr(prefix.size());
  }

  if (value.empty()) {
    throw std::runtime_error("Option " + std::string(shortOption) + " requires an argument");
  }
  return true;
}

std::size_t parseRepetitions(const std::string& value)
{
  std::size_t repetitions = 0;
  for (const char character : value) {
    if (character < '0' || character > '9') {
      throw std::runtime_error("Render count must be a positive integer: " + value);
    }
    const std::size_t digit = static_cast<std::size_t>(character - '0');
    if (repetitions > (std::numeric_limits<std::size_t>::max() - digit) / 10) {
      throw std::runtime_error("Render count must be between 1 and " + std::to_string(maxRepetitions) + ": " + value);
    }
    repetitions = repetitions * 10 + digit;
  }
  if (repetitions == 0 || repetitions > maxRepetitions) {
    throw std::runtime_error("Render count must be between 1 and " + std::to_string(maxRepetitions) + ": " + value);
  }
  return repetitions;
}

void addPartial(Options& options, const std::string& value)
{
  const std::size_t separator = value.find('=');
  if (separator == std::string::npos || separator == 0 || separator + 1 == value.size()) {
    throw std::runtime_error("Partial must use the form <name>=<file>: " + value);
  }

  const std::string name = value.substr(0, separator);
  const std::string path = value.substr(separator + 1);
  if (!options.partialPaths.emplace(name, path).second) {
    throw std::runtime_error("Duplicate partial name: " + name);
  }
}

Options parseOptions(const std::vector<std::string>& arguments)
{
  Options options;
  bool endOfOptions = false;
  for (std::size_t index = 0; index < arguments.size(); ++index) {
    const std::string& argument = arguments[index];
    if (!endOfOptions && argument == "--") {
      endOfOptions = true;
      continue;
    }
    if (endOfOptions) {
      throw std::runtime_error("Unexpected argument: " + argument);
    }
    if (argument == "-h" || argument == "--help") {
      options.action = Action::Help;
      return options;
    }
    if (argument == "-v" || argument == "--version") {
      options.action = Action::Version;
      return options;
    }
    if (argument == "-r") {
      options.deprecatedReadable = true;
      continue;
    }

    std::string value;
    if (readOptionValue(arguments, index, "-t", "--template", value)) {
      if (!options.templatePath.empty()) {
        throw std::runtime_error("Template file was specified more than once");
      }
      options.templatePath = std::move(value);
    } else if (readOptionValue(arguments, index, "-d", "--data", value)) {
      if (!options.dataPath.empty()) {
        throw std::runtime_error("Data file was specified more than once");
      }
      options.dataPath = std::move(value);
    } else if (readOptionValue(arguments, index, "-o", "--output", value)) {
      if (!options.outputPath.empty()) {
        throw std::runtime_error("Output file was specified more than once");
      }
      options.outputPath = std::move(value);
    } else if (readOptionValue(arguments, index, "-n", "--repeat", value)) {
      if (options.repetitionsSet) {
        throw std::runtime_error("Render count was specified more than once");
      }
      options.repetitions = parseRepetitions(value);
      options.repetitionsSet = true;
    } else if (readOptionValue(arguments, index, "-l", "--partial", value)) {
      addPartial(options, value);
    } else if (!argument.empty() && argument.front() == '-') {
      throw std::runtime_error("Unknown option: " + argument);
    } else {
      throw std::runtime_error("Unexpected argument: " + argument);
    }
  }

  if (options.templatePath.empty()) {
    throw std::runtime_error("A template file is required; use -t <file>");
  }
  return options;
}

std::string systemMessage(int errorNumber)
{
  if (errorNumber == 0) {
    return "I/O error";
  }
  return std::error_code(errorNumber, std::generic_category()).message();
}

std::string readFile(const std::string& path, std::string_view description, std::size_t maxBytes)
{
  errno = 0;
  std::ifstream input(path, std::ios::in | std::ios::binary);
  if (!input) {
    const int errorNumber = errno;
    throw std::runtime_error(
        "Cannot open " + std::string(description) + " '" + path + "': " + systemMessage(errorNumber));
  }

  std::string contents;
  std::array<char, 8192> buffer{};
  for (;;) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    if (count > 0) {
      const std::size_t bytes = static_cast<std::size_t>(count);
      if (contents.size() > maxBytes || bytes > maxBytes - contents.size()) {
        throw std::runtime_error(std::string(description) + " exceeds its input byte limit: " + path);
      }
      contents.append(buffer.data(), bytes);
    }
    if (input.eof()) {
      break;
    }
    if (!input) {
      throw std::runtime_error("Cannot read " + std::string(description) + " '" + path + "'");
    }
  }
  return contents;
}

DataFormat detectDataFormat(const std::string& path)
{
  const std::size_t directorySeparator = path.find_last_of("/\\");
  const std::size_t separator = path.find_last_of('.');
  std::string extension =
      separator == std::string::npos || (directorySeparator != std::string::npos && separator < directorySeparator)
      ? std::string()
      : path.substr(separator);
  for (char& character : extension) {
    if (character >= 'A' && character <= 'Z') {
      character = static_cast<char>(character - 'A' + 'a');
    }
  }
  if (extension == ".json") {
    return DataFormat::Json;
  }
  if (extension == ".yml" || extension == ".yaml") {
    return DataFormat::Yaml;
  }
  throw std::runtime_error("Unsupported data file type for '" + path + "'; expected .json, .yml, or .yaml");
}

mustache::Data readData(const Options& options, const mustache::Data::ParseLimits& limits)
{
  if (options.dataPath.empty()) {
    return mustache::Data::null();
  }
  const DataFormat format = detectDataFormat(options.dataPath);
  const std::string source = readFile(options.dataPath, "data file", limits.maxInputBytes);
  if (source.empty()) {
    throw std::runtime_error("Data file is empty: " + options.dataPath);
  }
  if (format == DataFormat::Json) {
    return mustache::Data::fromJSON(std::string_view(source), limits);
  }
  return mustache::Data::fromYAML(std::string_view(source), limits);
}

mustache::PartialMap readPartials(const Options& options, const mustache::Tokenizer::Limits& limits)
{
  mustache::PartialMap partials;
  for (const auto& entry : options.partialPaths) {
    const std::string source = readFile(entry.second, "partial '" + entry.first + "'", limits.maxInputBytes);
    partials.emplace(entry.first, mustache::compile(source, limits));
  }
  return partials;
}

void writeOutput(const std::string& path, const std::string& rendered, std::ostream& output)
{
  if (path.empty()) {
    output.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
    output.flush();
    if (!output) {
      throw std::runtime_error("Cannot write rendered output to stdout");
    }
    return;
  }

  errno = 0;
  std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file) {
    const int errorNumber = errno;
    throw std::runtime_error("Cannot open output file '" + path + "': " + systemMessage(errorNumber));
  }
  file.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
  file.close();
  if (!file) {
    throw std::runtime_error("Cannot write output file '" + path + "'");
  }
}

void flushStandardOutput(std::ostream& output)
{
  output.flush();
  if (!output) {
    throw std::runtime_error("Cannot write to stdout");
  }
}

void writeDeprecationWarning(std::ostream& error)
{
  error << "mustachec: warning: -r is deprecated and ignored\n";
  error.flush();
  if (!error) {
    throw std::runtime_error("Cannot write deprecation warning to stderr");
  }
}

int render(const Options& options, std::ostream& output)
{
  const mustache::Tokenizer::Limits tokenizerLimits;
  const mustache::Data::ParseLimits dataLimits;
  const mustache::RenderLimits renderLimits;

  const std::string source = readFile(options.templatePath, "template file", tokenizerLimits.maxInputBytes);
  const mustache::CompiledTemplate compiled = mustache::compile(source, tokenizerLimits);
  const mustache::PartialMap partials = readPartials(options, tokenizerLimits);
  const mustache::Data data = readData(options, dataLimits);

  std::string rendered;
  for (std::size_t iteration = 0; iteration < options.repetitions; ++iteration) {
    rendered = mustache::render(compiled, data, partials, renderLimits);
  }
  writeOutput(options.outputPath, rendered, output);
  return 0;
}

void reportFailure(std::ostream& error, const char * message) noexcept
{
  try {
    error << "mustachec: " << message << '\n';
  } catch (...) {
  }
}

void reportTokenizerFailure(std::ostream& error, const mustache::TokenizerException& exception) noexcept
{
  try {
    error << "mustachec: " << exception.what();
    if (exception.lineNo > 0 && exception.charNo > 0) {
      error << " at line " << exception.lineNo << ", character " << exception.charNo;
    }
    error << '\n';
  } catch (...) {
  }
}

} // namespace

int run(const std::vector<std::string>& arguments, std::ostream& output, std::ostream& error) noexcept
{
  try {
    const Options options = parseOptions(arguments);
    if (options.deprecatedReadable) {
      writeDeprecationWarning(error);
    }
    if (options.action == Action::Help) {
      showUsage(output);
      flushStandardOutput(output);
      return 0;
    }
    if (options.action == Action::Version) {
      showVersion(output);
      flushStandardOutput(output);
      return 0;
    }
    return render(options, output);
  } catch (const mustache::TokenizerException& exception) {
    reportTokenizerFailure(error, exception);
  } catch (const std::exception& exception) {
    reportFailure(error, exception.what());
  } catch (...) {
    reportFailure(error, "unknown failure");
  }
  return 1;
}

} // namespace mustache_cli
