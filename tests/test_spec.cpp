
#include "test_spec.hpp"
#include "./fixtures/lambdas.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

std::vector<std::unique_ptr<MustacheSpecTest>> tests;
int execNum = 1;
static const char * currentSuite;

int main(int argc, char * argv[])
{
  char * directory = NULL;

  // disable buffering
  setbuf(stdout, NULL);

  if (argc >= 2) {
    directory = argv[1];
  }
  if (directory == NULL) {
    directory = getenv("mustache_spec_dir");
  }
  if (directory == NULL && argc >= 2) {
    directory = argv[1];
  }
  if (directory == NULL) {
    std::cerr << "Requires at least one argument or that MUSTACHE_SPEC_DIR be set the the environment\n";
    return 1;
  }

  char * numStr = getenv("EXEC_NUM");
  if (numStr != NULL) {
    printf("%s\n", numStr);
    const std::string_view value(numStr);
    int parsed = 0;
    const std::from_chars_result result = std::from_chars(value.data(), value.data() + value.size(), parsed, 10);
    if (result.ec != std::errc() || result.ptr != value.data() + value.size() || parsed <= 0) {
      std::cerr << "Invalid EXEC_NUM: expected a positive decimal integer\n";
      return 1;
    }
    execNum = parsed;
  }

  std::vector<std::string> files;
  std::error_code directoryError;
  std::filesystem::directory_iterator iterator(directory, directoryError);
  const std::filesystem::directory_iterator end;
  for (; iterator != end; iterator.increment(directoryError)) {
    if (directoryError) {
      break;
    }
    const std::string file = iterator->path().filename().string();
    if (file.empty() || file.front() == '.' || iterator->path().extension() != ".json") {
      continue;
    }

    if (iterator->is_regular_file(directoryError)) {
      files.push_back(file);
    } else if (directoryError) {
      break;
    }
  }
  if (directoryError) {
    std::cerr << "Unable to open directory " << directory << std::endl;
    return 1;
  }

  std::sort(files.begin(), files.end());

  for (const std::string& file : files) {
    //if( file[0] == '~' ) continue; // Ignore lambdas
    currentSuite = file.c_str();
    mustache_test::specRecordSuiteFile(currentSuite);

    // Make filename
    const std::filesystem::path fileName = std::filesystem::path(directory) / file;

    std::ifstream pFile(fileName, std::ios::in | std::ios::binary);
    if (!pFile.is_open()) {
      std::cerr << "Unable to open file: " << fileName.string();
      continue;
    }

    // get length of file:
    pFile.seekg(0, pFile.end);
    const std::streamoff streamLength = pFile.tellg();
    if (streamLength < 0 || streamLength > std::numeric_limits<std::streamsize>::max() ||
        static_cast<std::uintmax_t>(streamLength) > std::numeric_limits<std::size_t>::max()) {
      std::cerr << "Invalid file size: " << fileName.string() << "\n";
      continue;
    }
    pFile.seekg(0, pFile.beg);

    // read file data
    std::string fileData(static_cast<std::size_t>(streamLength), '\0');
    if (!fileData.empty()) {
      pFile.read(fileData.data(), static_cast<std::streamsize>(streamLength));
    }
    if (!pFile) {
      std::cerr << "Unable to read file: " << fileName.string() << "\n";
      continue;
    }
    pFile.close();

    // parse the file
    std::cout << fileName.string() << "\n";
    try {
      parse_file(fileData.data(), fileData.size());
    } catch (const std::exception& error) {
      std::cerr << "Unable to load " << fileName.string() << ": " << error.what() << "\n";
      return 1;
    }
  }

  // Summarize
  int nPassed = 0;
  int nKnownFailures = 0;
  int nUnexpectedFailures = 0;
  int nUnexpectedPasses = 0;
  int nSkipped = 0;
  for (const std::unique_ptr<MustacheSpecTest>& test : tests) {
    if (test->skipped) {
      nSkipped++;
    } else if (test->knownFailure) {
      if (test->passed()) {
        nUnexpectedPasses++;
      } else {
        nKnownFailures++;
      }
    } else if (test->passed()) {
      nPassed++;
    } else {
      nUnexpectedFailures++;
    }
  }
  tests.clear();
  const bool inventoryValid = mustache_test::validateSpecInventory(std::cerr);
  int total = nPassed + nKnownFailures + nUnexpectedFailures + nUnexpectedPasses + nSkipped;
  std::cout << nPassed << " passed, " << nSkipped << " skipped, " << nKnownFailures << " known failures, "
            << nUnexpectedFailures << " unexpected failures, " << nUnexpectedPasses << " unexpected passes of " << total
            << " tests\n";
  return (!inventoryValid || nUnexpectedFailures > 0 || nUnexpectedPasses > 0 ? 1 : 0);
}

void parse_file(const char * fileData, std::size_t length)
{
  // The generated JSON fixtures make scalar types unambiguous. libyaml reads
  // their structure even when the library's optional JSON adapter is disabled.
  yaml_parser_t parser;
  yaml_document_t document;
  if (!yaml_parser_initialize(&parser)) {
    throw std::bad_alloc();
  }
  std::unique_ptr<yaml_parser_t, decltype(&yaml_parser_delete)> parserCleanup(&parser, &yaml_parser_delete);

  const unsigned char * input = reinterpret_cast<const unsigned char *>(fileData);

  yaml_parser_set_input_string(&parser, input, length);
  const bool loaded = yaml_parser_load(&parser, &document) != 0;
  parserCleanup.reset();
  if (!loaded) {
    throw std::runtime_error("Unable to parse specification fixture");
  }
  const std::unique_ptr<yaml_document_t, decltype(&yaml_document_delete)> documentCleanup(
      &document, &yaml_document_delete);
  mustache_spec_parse_document(&document);
}

void mustache_spec_parse_document(yaml_document_t * document)
{
  yaml_node_t * node = yaml_document_get_root_node(document);
  if (node == nullptr || node->type != YAML_MAPPING_NODE) {
    throw std::runtime_error("Specification fixture root must be an object");
  }

  yaml_node_pair_t * pair;
  for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t * keyNode = yaml_document_get_node(document, pair->key);
    yaml_node_t * valueNode = yaml_document_get_node(document, pair->value);
    const std::string_view keyValue(
        reinterpret_cast<const char *>(keyNode->data.scalar.value), keyNode->data.scalar.length);
    if (keyValue == "tests" && valueNode->type == YAML_SEQUENCE_NODE) {
      mustache_spec_parse_tests(document, valueNode);
    }
  }
}

void mustache_spec_parse_tests(yaml_document_t * document, yaml_node_t * node)
{
  if (node->type != YAML_SEQUENCE_NODE) {
    return;
  }

  yaml_node_item_t * item;
  for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
    yaml_node_t * valueNode = yaml_document_get_node(document, *item);
    if (valueNode->type == YAML_MAPPING_NODE) {
      mustache_spec_parse_test(document, valueNode);
    }
  }
}

void mustache_spec_parse_test(yaml_document_t * document, yaml_node_t * node)
{
  if (node->type != YAML_MAPPING_NODE) {
    return;
  }

  std::unique_ptr<MustacheSpecTest> test = std::make_unique<MustacheSpecTest>();
  test->suite.assign(currentSuite);

  // Read the name first so unsupported tests can be counted without parsing
  // data or partial structures that depend on the unsupported feature.
  yaml_node_pair_t * pair;
  for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; ++pair) {
    yaml_node_t * keyNode = yaml_document_get_node(document, pair->key);
    yaml_node_t * valueNode = yaml_document_get_node(document, pair->value);
    const std::string_view keyValue(
        reinterpret_cast<const char *>(keyNode->data.scalar.value), keyNode->data.scalar.length);
    if (keyValue == "name" && valueNode->type == YAML_SCALAR_NODE) {
      test->name.assign(reinterpret_cast<char *>(valueNode->data.scalar.value), valueNode->data.scalar.length);
      break;
    }
  }

  mustache_test::specRecordSuiteTest(test->suite);
  mustache_test::SpecExpectation expectation = mustache_test::specExpectationFor(test->suite, test->name);
  test->expectationReason.assign(expectation.reason);
  test->skipped = expectation.outcome == mustache_test::SpecExpectedSkip;
  test->knownFailure = expectation.outcome == mustache_test::SpecExpectedFailure;
  if (test->skipped) {
    test->print();
    tests.push_back(std::move(test));
    return;
  }

  for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t * keyNode = yaml_document_get_node(document, pair->key);
    yaml_node_t * valueNode = yaml_document_get_node(document, pair->value);
    const std::string_view keyValue(
        reinterpret_cast<const char *>(keyNode->data.scalar.value), keyNode->data.scalar.length);

    if (keyValue == "data") {
      mustache_spec_parse_data(document, valueNode, &test->data);
      continue;
    }

    if (valueNode->type == YAML_SCALAR_NODE) {
      const std::string_view valueValue(
          reinterpret_cast<const char *>(valueNode->data.scalar.value), valueNode->data.scalar.length);
      if (keyValue == "name") {
        test->name.assign(valueValue);
      } else if (keyValue == "desc") {
        test->desc.assign(valueValue);
      } else if (keyValue == "template") {
        test->tmpl.assign(valueValue);
      } else if (keyValue == "expected") {
        test->expected.assign(valueValue);
      }
    } else if (valueNode->type == YAML_MAPPING_NODE) {
      if (keyValue == "partials") {
        mustache_spec_parse_partials(document, valueNode, &test->partials);
      }
    }
  }

  mustache::Mustache mustache;
  bool isLambdaSuite = 0 == strcmp(currentSuite, "~lambdas.json");

  // Load lambdas?
  if (isLambdaSuite) {
    load_lambdas_into_test_data(&test->data, test->name);
  }

  // Tokenize
  mustache::Node root;
  mustache.tokenize(&test->tmpl, &root);

  // Execute the test
  for (int i = 0; i < execNum; i++) {
    test->output.clear();
    mustache.render(&root, &test->data, &test->partials, &test->output);
  }

  // Output result
  test->print();
  tests.push_back(std::move(test));
}

void mustache_spec_parse_data(yaml_document_t * document, yaml_node_t * node, mustache::Data * data)
{
  if (node->type == YAML_MAPPING_NODE) {
    yaml_node_pair_t * pair;

    data->init(mustache::Data::TypeMap, 0);

    for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
      yaml_node_t * keyNode = yaml_document_get_node(document, pair->key);
      yaml_node_t * valueNode = yaml_document_get_node(document, pair->value);
      const std::string keyValue(
          reinterpret_cast<const char *>(keyNode->data.scalar.value), keyNode->data.scalar.length);
      mustache::Data child;
      mustache_spec_parse_data(document, valueNode, &child);
      data->set(keyValue, std::move(child));
    }
  } else if (node->type == YAML_SEQUENCE_NODE) {
    yaml_node_item_t * item;
    const std::ptrdiff_t itemCount = node->data.sequence.items.top - node->data.sequence.items.start;
    if (itemCount < 0 || itemCount > static_cast<std::ptrdiff_t>(std::numeric_limits<int>::max())) {
      throw std::runtime_error("Invalid specification array size");
    }
    data->init(mustache::Data::TypeArray, static_cast<int>(itemCount));
    for (item = node->data.sequence.items.start; item < node->data.sequence.items.top; item++) {
      mustache::Data child;
      yaml_node_t * valueNode = yaml_document_get_node(document, *item);
      mustache_spec_parse_data(document, valueNode, &child);
      data->push_back(std::move(child));
    }
  } else if (node->type == YAML_SCALAR_NODE) {
    const std::string_view value(reinterpret_cast<const char *>(node->data.scalar.value), node->data.scalar.length);
    if (node->data.scalar.style == YAML_DOUBLE_QUOTED_SCALAR_STYLE) {
      *data = mustache::Data::string(std::string(value));
    } else if (value == "null") {
      *data = mustache::Data::null();
    } else if (value == "true" || value == "false") {
      *data = mustache::Data::boolean(value == "true");
    } else if (value.find_first_of(".eE") != std::string_view::npos) {
      // Older libc++ versions lack floating-point from_chars. A classic-locale
      // stream keeps JSON decimal parsing portable and locale-independent.
      std::istringstream stream{std::string(value)};
      stream.imbue(std::locale::classic());
      double parsed = 0;
      if (!(stream >> std::noskipws >> parsed) || !stream.eof()) {
        throw std::runtime_error("Unsupported specification number");
      }
      *data = mustache::Data::floating(parsed);
    } else {
      std::int64_t parsed = 0;
      const char * end = value.data() + value.size();
      const auto result = std::from_chars(value.data(), end, parsed);
      if (result.ec != std::errc() || result.ptr != end) {
        throw std::runtime_error("Unsupported specification number");
      }
      *data = mustache::Data::integer(parsed);
    }
  }
}

void mustache_spec_parse_partials(yaml_document_t * document, yaml_node_t * node, mustache::Node::Partials * partials)
{
  if (node->type != YAML_MAPPING_NODE) {
    return;
  }

  mustache::Mustache mustache;
  yaml_node_pair_t * pair;

  for (pair = node->data.mapping.pairs.start; pair < node->data.mapping.pairs.top; pair++) {
    yaml_node_t * keyNode = yaml_document_get_node(document, pair->key);
    yaml_node_t * valueNode = yaml_document_get_node(document, pair->value);
    const std::string ckey(reinterpret_cast<const char *>(keyNode->data.scalar.value), keyNode->data.scalar.length);
    const std::string tmpl(reinterpret_cast<const char *>(valueNode->data.scalar.value), valueNode->data.scalar.length);

    std::unique_ptr<mustache::Node>& partial = (*partials)[ckey];
    partial = std::make_unique<mustache::Node>();
    mustache.tokenize(tmpl, partial.get());
  }
}
