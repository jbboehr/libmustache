#ifndef MUSTACHE_TEST_SPEC_EXPECTATIONS_HPP
#define MUSTACHE_TEST_SPEC_EXPECTATIONS_HPP

#include <cstddef>
#include <cstring>
#include <ostream>
#include <string>

namespace mustache_test {

enum SpecExpectedOutcome {
  SpecExpectedPass,
  SpecExpectedFailure,
  SpecExpectedSkip
};

struct SpecExpectation {
  SpecExpectedOutcome outcome;
  const char * reason;
};

struct SpecExpectationRecord {
  const char * suite;
  const char * name;
  SpecExpectedOutcome outcome;
  const char * reason;
  std::size_t expectedMatches;
  std::size_t matches;
};

struct SpecSuiteRecord {
  const char * suite;
  std::size_t expectedTests;
  std::size_t tests;
  std::size_t files;
};

static const char standaloneSectionReason[] =
    "standalone section tag whitespace is not implemented";
static const char standaloneInvertedReason[] =
    "standalone inverted-section tag whitespace is not implemented";
static const char standaloneCommentReason[] =
    "standalone comment tag whitespace is not implemented";
static const char standaloneDelimiterReason[] =
    "standalone delimiter tag whitespace is not implemented";
static const char partialIndentationReason[] =
    "standalone partial stripping and indentation are not implemented";

// This is the executable deviation ledger for mustache/spec at 5d3b58e.
// A known failure that starts passing is an unexpected pass until this entry is
// removed. A missing or duplicated entry fails the inventory validation.
static SpecExpectationRecord specExpectations[] = {
    {"partials.yml", "Standalone Line Endings", SpecExpectedFailure,
        partialIndentationReason, 1, 0},
    {"partials.yml", "Standalone Without Previous Line", SpecExpectedFailure,
        partialIndentationReason, 1, 0},
    {"partials.yml", "Standalone Without Newline", SpecExpectedFailure,
        partialIndentationReason, 1, 0},
    {"partials.yml", "Standalone Indentation", SpecExpectedFailure,
        partialIndentationReason, 1, 0},

    {"sections.yml", "Deeply Nested Contexts", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},
    {"sections.yml", "Doubled", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},
    {"sections.yml", "Standalone Lines", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},
    {"sections.yml", "Indented Standalone Lines", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},
    {"sections.yml", "Standalone Line Endings", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},
    {"sections.yml", "Standalone Without Previous Line", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},
    {"sections.yml", "Standalone Without Newline", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},

    {"inverted.yml", "Doubled", SpecExpectedFailure,
        standaloneInvertedReason, 1, 0},
    {"inverted.yml", "Standalone Lines", SpecExpectedFailure,
        standaloneInvertedReason, 1, 0},
    {"inverted.yml", "Standalone Indented Lines", SpecExpectedFailure,
        standaloneInvertedReason, 1, 0},
    {"inverted.yml", "Standalone Line Endings", SpecExpectedFailure,
        standaloneInvertedReason, 1, 0},
    {"inverted.yml", "Standalone Without Previous Line", SpecExpectedFailure,
        standaloneInvertedReason, 1, 0},
    {"inverted.yml", "Standalone Without Newline", SpecExpectedFailure,
        standaloneInvertedReason, 1, 0},

    {"comments.yml", "Standalone", SpecExpectedFailure,
        standaloneCommentReason, 1, 0},
    {"comments.yml", "Indented Standalone", SpecExpectedFailure,
        standaloneCommentReason, 1, 0},
    {"comments.yml", "Standalone Line Endings", SpecExpectedFailure,
        standaloneCommentReason, 1, 0},
    {"comments.yml", "Standalone Without Previous Line", SpecExpectedFailure,
        standaloneCommentReason, 1, 0},
    {"comments.yml", "Standalone Without Newline", SpecExpectedFailure,
        standaloneCommentReason, 1, 0},
    {"comments.yml", "Multiline Standalone", SpecExpectedFailure,
        standaloneCommentReason, 1, 0},
    {"comments.yml", "Indented Multiline Standalone", SpecExpectedFailure,
        standaloneCommentReason, 1, 0},

    {"~lambdas.yml", "Interpolation - Alternate Delimiters",
        SpecExpectedFailure, standaloneDelimiterReason, 1, 0},

    {"delimiters.yml", "Sections", SpecExpectedFailure,
        standaloneSectionReason, 1, 0},
    {"delimiters.yml", "Inverted Sections", SpecExpectedFailure,
        standaloneInvertedReason, 1, 0},
    {"delimiters.yml", "Partial Inheritence", SpecExpectedFailure,
        standaloneDelimiterReason, 1, 0},
    {"delimiters.yml", "Standalone Tag", SpecExpectedFailure,
        standaloneDelimiterReason, 1, 0},
    {"delimiters.yml", "Indented Standalone Tag", SpecExpectedFailure,
        standaloneDelimiterReason, 1, 0},
    {"delimiters.yml", "Standalone Line Endings", SpecExpectedFailure,
        standaloneDelimiterReason, 1, 0},
    {"delimiters.yml", "Standalone Without Previous Line",
        SpecExpectedFailure, standaloneDelimiterReason, 1, 0},
    {"delimiters.yml", "Standalone Without Newline", SpecExpectedFailure,
        standaloneDelimiterReason, 1, 0},

    {"sections.yml", "Implicit Iterator - Array", SpecExpectedSkip,
        "nested array iteration is not implemented", 1, 0},
    {"~dynamic-names.yml", NULL, SpecExpectedSkip,
        "dynamic partial names are not implemented", 21, 0},
    {"~inheritance.yml", NULL, SpecExpectedSkip,
        "template inheritance is not implemented", 22, 0}
};

static SpecSuiteRecord specSuites[] = {
    {"comments.yml", 12, 0, 0},
    {"delimiters.yml", 14, 0, 0},
    {"interpolation.yml", 39, 0, 0},
    {"inverted.yml", 22, 0, 0},
    {"partials.yml", 11, 0, 0},
    {"sections.yml", 30, 0, 0},
    {"~dynamic-names.yml", 21, 0, 0},
    {"~inheritance.yml", 22, 0, 0},
    {"~lambdas.yml", 10, 0, 0}
};

static std::size_t unexpectedSuiteTests = 0;
static std::size_t unexpectedSuiteFiles = 0;

static SpecExpectation specExpectationFor(
    const std::string& suite, const std::string& name)
{
  const std::size_t count =
      sizeof(specExpectations) / sizeof(specExpectations[0]);
  for( std::size_t i = 0; i < count; ++i ) {
    SpecExpectationRecord& record = specExpectations[i];
    if( suite == record.suite &&
        (record.name == NULL || name == record.name) ) {
      ++record.matches;
      SpecExpectation result = {record.outcome, record.reason};
      return result;
    }
  }

  SpecExpectation result = {SpecExpectedPass, ""};
  return result;
}

static void specRecordSuiteTest(const std::string& suite)
{
  const std::size_t count = sizeof(specSuites) / sizeof(specSuites[0]);
  for( std::size_t i = 0; i < count; ++i ) {
    if( suite == specSuites[i].suite ) {
      ++specSuites[i].tests;
      return;
    }
  }
  ++unexpectedSuiteTests;
}

static void specRecordSuiteFile(const std::string& suite)
{
  const std::size_t count = sizeof(specSuites) / sizeof(specSuites[0]);
  for( std::size_t i = 0; i < count; ++i ) {
    if( suite == specSuites[i].suite ) {
      ++specSuites[i].files;
      return;
    }
  }
  ++unexpectedSuiteFiles;
}

static bool validateSpecInventory(std::ostream& output)
{
  bool valid = true;
  const std::size_t expectationCount =
      sizeof(specExpectations) / sizeof(specExpectations[0]);
  for( std::size_t i = 0; i < expectationCount; ++i ) {
    const SpecExpectationRecord& record = specExpectations[i];
    if( record.matches != record.expectedMatches ) {
      output << "Expectation inventory mismatch for " << record.suite;
      if( record.name != NULL ) {
        output << " / " << record.name;
      }
      output << ": expected " << record.expectedMatches << ", saw "
             << record.matches << "\n";
      valid = false;
    }
  }

  const std::size_t suiteCount = sizeof(specSuites) / sizeof(specSuites[0]);
  for( std::size_t i = 0; i < suiteCount; ++i ) {
    const SpecSuiteRecord& record = specSuites[i];
    if( record.files != 1 ) {
      output << "Spec file inventory mismatch for " << record.suite
             << ": expected 1, saw " << record.files << "\n";
      valid = false;
    }
    if( record.tests != record.expectedTests ) {
      output << "Spec inventory mismatch for " << record.suite
             << ": expected " << record.expectedTests << ", saw "
             << record.tests << "\n";
      valid = false;
    }
  }

  if( unexpectedSuiteTests != 0 ) {
    output << "Found " << unexpectedSuiteTests
           << " tests in suites missing from the inventory\n";
    valid = false;
  }
  if( unexpectedSuiteFiles != 0 ) {
    output << "Found " << unexpectedSuiteFiles
           << " spec files missing from the inventory\n";
    valid = false;
  }

  return valid;
}

} // namespace mustache_test

#endif
