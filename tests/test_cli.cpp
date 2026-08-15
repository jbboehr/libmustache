#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <streambuf>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "cli.hpp"
#include "mustache_config.h"
#include "mustache.hpp"

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
  if( !condition ) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

bool makeDirectory(const std::string& path)
{
#ifdef _WIN32
  return _mkdir(path.c_str()) == 0;
#else
  return mkdir(path.c_str(), 0700) == 0;
#endif
}

void removeDirectory(const std::string& path)
{
#ifdef _WIN32
  static_cast<void>(_rmdir(path.c_str()));
#else
  static_cast<void>(rmdir(path.c_str()));
#endif
}

class TestDirectory {
  public:
    TestDirectory()
    {
      const auto timestamp = std::chrono::steady_clock::now()
          .time_since_epoch().count();
      for( unsigned int attempt = 0; attempt < 100; ++attempt ) {
        root_ = "mustache-cli-test-" + std::to_string(timestamp) + "-" +
            std::to_string(attempt);
        if( makeDirectory(root_) ) {
          return;
        }
        if( errno != EEXIST ) {
          break;
        }
      }
      throw std::runtime_error("Cannot create CLI test directory");
    }

    TestDirectory(const TestDirectory&) = delete;
    TestDirectory& operator=(const TestDirectory&) = delete;

    ~TestDirectory()
    {
      for( auto path = paths_.rbegin(); path != paths_.rend(); ++path ) {
        static_cast<void>(std::remove(path->c_str()));
      }
      removeDirectory(root_);
    }

    std::string path(const std::string& name)
    {
      const std::string result = root_ + "/" + name;
      paths_.push_back(result);
      return result;
    }

    const std::string& root() const noexcept
    {
      return root_;
    }

  private:
    std::string root_;
    std::vector<std::string> paths_;
};

void writeFile(const std::string& path, const std::string& contents)
{
  std::ofstream output(
      path, std::ios::out | std::ios::binary | std::ios::trunc);
  output.write(contents.data(),
      static_cast<std::streamsize>(contents.size()));
  output.close();
  if( !output ) {
    throw std::runtime_error("Cannot write CLI test fixture: " + path);
  }
}

std::string readFile(const std::string& path)
{
  std::ifstream input(path, std::ios::in | std::ios::binary);
  std::ostringstream contents;
  contents << input.rdbuf();
  if( !input.eof() && !input ) {
    throw std::runtime_error("Cannot read CLI test output: " + path);
  }
  return contents.str();
}

struct Invocation {
  int result;
  std::string output;
  std::string error;
};

Invocation invoke(std::vector<std::string> arguments)
{
  std::ostringstream output;
  std::ostringstream error;
  const int result = mustache_cli::run(arguments, output, error);
  return {result, output.str(), error.str()};
}

class RejectingStreamBuffer : public std::streambuf {
  protected:
    int_type overflow(int_type) override
    {
      return traits_type::eof();
    }
};

class FlushRejectingStreamBuffer : public std::stringbuf {
  protected:
    int sync() override
    {
      return -1;
    }
};

void expectSuccess(const Invocation& invocation,
    const std::string& expectedOutput, const std::string& description)
{
  expect(invocation.result == 0, description + " returned failure");
  expect(invocation.output == expectedOutput,
      description + " produced unexpected stdout");
  expect(invocation.error.empty(),
      description + " produced unexpected stderr: " + invocation.error);
}

void expectFailure(const Invocation& invocation,
    const std::string& expectedError, const std::string& description)
{
  expect(invocation.result != 0, description + " returned success");
  expect(invocation.output.empty(),
      description + " produced unexpected stdout");
  expect(invocation.error.rfind("mustachec: ", 0) == 0,
      description + " did not produce a prefixed diagnostic");
  expect(invocation.error.find(expectedError) != std::string::npos,
      description + " did not report '" + expectedError + "': " +
          invocation.error);
}

void testHelpAndVersion()
{
  const Invocation help = invoke({"--help"});
  expect(help.result == 0, "help returned failure");
  expect(help.output.find(
      "Usage: mustachec -t <template> [-d <data>] [options]") !=
      std::string::npos, "help omitted the render-only usage");
  expect(help.output.find("Compile") == std::string::npos,
      "help still advertised an unsupported compile mode");
  expect(help.error.empty(), "help produced a diagnostic");

  const Invocation version = invoke({"-v"});
  expect(version.result == 0, "version returned failure");
  expect(version.output.find(
      std::string("mustachec ") + mustache_version() + "\n") == 0,
      "version omitted the executable name or package version");
#ifdef MUSTACHE_HAVE_LIBJSON
  expect(version.output.find("JSON support: json-c\n") != std::string::npos,
      "version reported the wrong JSON adapter");
#else
  expect(version.output.find("JSON support: none\n") != std::string::npos,
      "version reported unavailable JSON support");
#endif
#ifdef MUSTACHE_HAVE_LIBYAML
  expect(version.output.find("YAML support: libyaml\n") != std::string::npos,
      "version reported the wrong YAML adapter");
#else
  expect(version.output.find("YAML support: none\n") != std::string::npos,
      "version reported unavailable YAML support");
#endif
  expect(version.error.empty(), "version produced a diagnostic");

  RejectingStreamBuffer outputBuffer;
  std::ostream failedOutput(&outputBuffer);
  failedOutput.exceptions(std::ios::badbit | std::ios::failbit);
  std::ostringstream outputError;
  expect(mustache_cli::run({"--help"}, failedOutput, outputError) != 0,
      "failed stdout escaped the CLI boundary");
  expect(outputError.str().find("mustachec: ") == 0,
      "failed stdout did not produce a controlled diagnostic");

  for( const std::string& option : {std::string("--help"),
           std::string("--version")} ) {
    FlushRejectingStreamBuffer flushBuffer;
    std::ostream flushFailedOutput(&flushBuffer);
    std::ostringstream flushError;
    expect(mustache_cli::run(
               {option}, flushFailedOutput, flushError) != 0,
        option + " flush failure escaped the CLI boundary");
    expect(flushError.str().find("mustachec: ") == 0,
        option + " flush failure lacked a controlled diagnostic");
  }

  RejectingStreamBuffer errorBuffer;
  std::ostream failedError(&errorBuffer);
  failedError.exceptions(std::ios::badbit | std::ios::failbit);
  std::ostringstream ignoredOutput;
  expect(mustache_cli::run({}, ignoredOutput, failedError) != 0,
      "failed stderr escaped the CLI boundary");
}

void testSuccessfulRendering(TestDirectory& directory)
{
  const std::string templatePath = directory.path("success.mustache");
  const std::string dataPath = directory.path("success.json");
  const std::string partialPath = directory.path("tail.mustache");
  const std::string outputPath = directory.path("rendered.txt");
  writeFile(templatePath, "Hello {{name}} {{>tail}}");
  writeFile(dataPath, "{\"name\":\"World\"}");
  writeFile(partialPath, "({{name}})");

  expectSuccess(invoke({"-t", templatePath, "-d", dataPath,
                    "-l", "tail=" + partialPath}),
      "Hello World (World)", "JSON render with a partial");

  writeFile(outputPath, "stale output that must be truncated");
  const Invocation fileOutput = invoke({"-t" + templatePath,
      "-d" + dataPath, "-ltail=" + partialPath, "-n2",
      "-o" + outputPath});
  expectSuccess(fileOutput, std::string(), "render to an output file");
  expect(readFile(outputPath) == "Hello World (World)",
      "output file was not truncated to the final repeated render");

  const std::string staticTemplate = directory.path("static.mustache");
  writeFile(staticTemplate, "static");
  expectSuccess(invoke({"-t", staticTemplate}), "static",
      "render with the default null data");
  expectSuccess(invoke({"-t", staticTemplate, "--"}), "static",
      "render with an end-of-options marker");

  const Invocation deprecated = invoke({"-r", "-t", staticTemplate});
  expect(deprecated.result == 0, "deprecated -r compatibility failed");
  expect(deprecated.output == "static",
      "deprecated -r changed rendered output");
  expect(deprecated.error.find("-r is deprecated and ignored") !=
      std::string::npos, "deprecated -r omitted its warning");

  const std::string yamlTemplate = directory.path("yaml.mustache");
  const std::string yamlData = directory.path("data.yaml");
  writeFile(yamlTemplate, "{{value}}");
  writeFile(yamlData, "value: yaml\n");
  expectSuccess(invoke({"--template=" + yamlTemplate,
                    "--data=" + yamlData}),
      "yaml", "YAML render through long options");

  const std::string uppercaseData = directory.path("uppercase.JSON");
  writeFile(uppercaseData, "{\"value\":\"uppercase\"}");
  expectSuccess(invoke({"-t", yamlTemplate, "-d", uppercaseData}),
      "uppercase", "case-insensitive data extension");

  const std::string emptyTemplate = directory.path("empty.mustache");
  writeFile(emptyTemplate, std::string());
  expectSuccess(invoke({"-t", emptyTemplate, "-d", dataPath}),
      std::string(), "empty template render");

  const std::string boundaryTemplate = directory.path("boundary.mustache");
  const std::string boundaryPrefix(8190, 'x');
  writeFile(boundaryTemplate, boundaryPrefix + "{{name}}");
  expectSuccess(invoke({"-t", boundaryTemplate, "-d", dataPath}),
      boundaryPrefix + "World", "template spanning the read chunk boundary");

  const std::string binaryTemplate = directory.path("binary.mustache");
  const std::string binaryOutput = directory.path("binary-output.txt");
  const std::string binarySource("pre\0{{name}}post", 16);
  const std::string binaryExpected("pre\0Worldpost", 13);
  writeFile(binaryTemplate, binarySource);
  expectSuccess(invoke({"-t", binaryTemplate, "-d", dataPath}),
      binaryExpected, "template containing an embedded NUL");
  expectSuccess(invoke({"-t", binaryTemplate, "-d", dataPath,
                    "-o", binaryOutput}),
      std::string(), "binary render to an output file");
  expect(readFile(binaryOutput) == binaryExpected,
      "output file did not preserve the embedded NUL");

  FlushRejectingStreamBuffer flushBuffer;
  std::ostream flushFailedOutput(&flushBuffer);
  std::ostringstream flushError;
  expect(mustache_cli::run({"-t", staticTemplate},
             flushFailedOutput, flushError) != 0,
      "render flush failure escaped the CLI boundary");
  expect(flushError.str().find("mustachec: ") == 0,
      "render flush failure lacked a controlled diagnostic");
}

void testArgumentFailures(const std::string& templatePath,
    const std::string& dataPath,
    const std::string& partialPath)
{
  expectFailure(invoke({}), "A template file is required",
      "missing template option");
  expectFailure(invoke({"-e"}), "Unknown option: -e",
      "obsolete execute option");
  expectFailure(invoke({"operand"}), "Unexpected argument: operand",
      "unexpected positional argument");
  expectFailure(invoke({"-t"}), "Option -t requires an argument",
      "option without a value");
  expectFailure(invoke({"-t=" + templatePath}), "does not use '='",
      "short option with an equals sign");
  expectFailure(invoke({"-t", templatePath, "-t", templatePath,
                    "-d", dataPath}),
      "Template file was specified more than once",
      "duplicate template option");
  expectFailure(invoke({"-t", templatePath, "-d", dataPath,
                    "-l", "invalid"}),
      "Partial must use the form <name>=<file>",
      "invalid partial option");
  expectFailure(invoke({"-t", templatePath, "-d", dataPath,
                    "-l", "same=" + partialPath,
                    "-l", "same=" + partialPath}),
      "Duplicate partial name: same", "duplicate partial option");

  expectFailure(invoke({"-t", templatePath, "-n", "1",
                    "--repeat=2"}),
      "Render count was specified more than once",
      "duplicate repeat option");

  expectFailure(invoke({"-t", templatePath, "-n", "invalid"}),
      "Render count must be a positive integer",
      "nonnumeric repeat count");
  for( const std::string& value : {std::string("0"),
           std::string("1000001"),
           std::string("999999999999999999999999999999999999")} ) {
    expectFailure(invoke({"-t", templatePath, "-n", value}),
        "Render count must be between 1 and 1000000",
        "out-of-range repeat count " + value);
  }
}

void testFileAndParseFailures(TestDirectory& directory,
    const std::string& validTemplate, const std::string& validData)
{
  const std::string missingTemplate = directory.path("missing.mustache");
  const std::string missingData = directory.path("missing.json");
  const std::string missingPartial = directory.path("missing-partial.mustache");
  expectFailure(invoke({"-t", missingTemplate, "-d", validData}),
      "Cannot open template file", "missing template file");
  expectFailure(invoke({"-t", validTemplate, "-d", missingData}),
      "Cannot open data file", "missing data file");
  expectFailure(invoke({"-t", validTemplate, "-d", validData,
                    "-l", "missing=" + missingPartial}),
      "Cannot open partial 'missing'", "missing partial file");

  const std::string emptyData = directory.path("empty.json");
  writeFile(emptyData, std::string());
  expectFailure(invoke({"-t", validTemplate, "-d", emptyData}),
      "Data file is empty", "empty data file");

  const std::string invalidJson = directory.path("invalid.json");
  writeFile(invalidJson, "{]");
  expectFailure(invoke({"-t", validTemplate, "-d", invalidJson}),
      "Invalid JSON data", "invalid JSON data");

  const std::string invalidYaml = directory.path("invalid.yml");
  writeFile(invalidYaml, "value: [unterminated\n");
  expectFailure(invoke({"-t", validTemplate, "-d", invalidYaml}),
      "yaml", "invalid YAML data");

  const std::string unknownData = directory.path("data.txt");
  writeFile(unknownData, "{}");
  expectFailure(invoke({"-t", validTemplate, "-d", unknownData}),
      "Unsupported data file type", "unsupported data file extension");

  expectFailure(invoke({"-t", validTemplate, "-d", validData,
                    "-o", directory.root()}),
      "Cannot open output file", "unwritable output path");
}

void testCompileAndRenderFailures(TestDirectory& directory,
    const std::string& validData)
{
  const std::string invalidTemplate = directory.path("invalid.mustache");
  writeFile(invalidTemplate, "{{#open}}");
  expectFailure(invoke({"-t", invalidTemplate, "-d", validData}),
      "Unclosed section", "template tokenization failure");

  const std::string recursiveTemplate =
      directory.path("recursive.mustache");
  const std::string recursivePartial =
      directory.path("recursive-partial.mustache");
  writeFile(recursiveTemplate, "{{>loop}}");
  writeFile(recursivePartial, "{{>loop}}");
  expectFailure(invoke({"-t", recursiveTemplate, "-d", validData,
                    "-l", "loop=" + recursivePartial}),
      "Render nesting limit exceeded", "recursive partial render failure");
}

} // namespace

int main()
{
  try {
    TestDirectory directory;
    const std::string validTemplate = directory.path("base.mustache");
    const std::string validData = directory.path("base.json");
    const std::string validPartial = directory.path("base-partial.mustache");
    writeFile(validTemplate, "{{value}}");
    writeFile(validData, "{\"value\":\"ok\"}");
    writeFile(validPartial, "partial");

    testHelpAndVersion();
    testSuccessfulRendering(directory);
    testArgumentFailures(validTemplate, validData, validPartial);
    testFileAndParseFailures(directory, validTemplate, validData);
    testCompileAndRenderFailures(directory, validData);
  } catch( const std::exception& exception ) {
    std::cerr << "FAIL: CLI test setup failed: " << exception.what() << '\n';
    ++failures;
  }
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
