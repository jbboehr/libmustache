#include "mustache_config.h"

#if !defined(MUSTACHE_HAVE_ARCHIVED_TEMPLATES)
#error "test_cista_version_mutation requires archived-template support"
#endif

#include "archived_template.hpp"
#include "exception.hpp"
#include "mustache.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> readArchive(const char * path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("unable to open the native baseline archive");
  }
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string readTag(const char * path)
{
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("unable to open the native baseline tag");
  }
  std::string tag;
  std::getline(stream, tag);
  if (!stream || tag.empty()) {
    throw std::runtime_error("unable to read the native baseline tag");
  }
  return tag;
}

void writeArchive(const char * path, const std::vector<std::uint8_t>& archive)
{
  std::ofstream stream(path, std::ios::binary);
  stream.write(reinterpret_cast<const char *>(archive.data()), static_cast<std::streamsize>(archive.size()));
  if (!stream) {
    throw std::runtime_error("unable to write the native mutation archive");
  }
}

void writeTag(const char * path, std::string_view tag)
{
  std::ofstream stream(path);
  stream << tag << '\n';
  if (!stream) {
    throw std::runtime_error("unable to write the native mutation tag");
  }
}

int produce(const char * archivePath, const char * tagPath)
{
  const mustache::CompiledTemplate compiled = mustache::compile("compatibility mutation");
  writeArchive(archivePath, mustache::serializeArchivedTemplate(compiled));
  writeTag(tagPath, mustache::archivedTemplateCompatibilityTag());
  return 0;
}

int reject(const char * archivePath, const char * tagPath)
{
  const std::vector<std::uint8_t> archive = readArchive(archivePath);
  const std::string foreignTag = readTag(tagPath);
  if (mustache::archivedTemplateCompatibilityTag() == foreignTag) {
    std::fprintf(stderr, "the Cista hash mutation did not change the native compatibility domain\n");
    return 1;
  }

  try {
    static_cast<void>(mustache::loadArchivedTemplate(archive));
  } catch (const mustache::ArchivedTemplateException& exception) {
    if (exception.reason() == mustache::ArchivedTemplateError::UnsupportedFormat &&
        std::string_view(exception.what()) == "Unsupported libmustache archive compatibility") {
      return 0;
    }
    std::fprintf(stderr, "the foreign native archive was rejected at the wrong boundary: %s\n", exception.what());
    return 1;
  }
  std::fprintf(stderr, "a foreign Cista hash domain read the native archive\n");
  return 1;
}

} // namespace

int main(int argc, char ** argv)
{
  try {
    if (argc == 4 && std::string_view(argv[1]) == "produce") {
      return produce(argv[2], argv[3]);
    }
    if (argc == 4 && std::string_view(argv[1]) == "reject") {
      return reject(argv[2], argv[3]);
    }
    std::fprintf(stderr, "expected produce or reject plus archive and tag paths\n");
    return 1;
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "unable to exercise the Cista version mutation: %s\n", exception.what());
    return 1;
  }
}
