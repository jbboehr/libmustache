#include "mustache.hpp"

#if defined(MUSTACHE_CISTA_BENCHMARK)
#include "cista-archive.hpp"
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <malloc.h>
#endif

namespace allocation_counter {

thread_local bool enabled = false;
thread_local std::uint64_t allocations = 0;
thread_local std::uint64_t bytes = 0;

void record(std::size_t size) noexcept
{
  if (enabled) {
    ++allocations;
    bytes += size;
  }
}

void * allocate(std::size_t size)
{
  record(size);
  if (void * pointer = std::malloc(size == 0 ? 1 : size)) {
    return pointer;
  }
  throw std::bad_alloc();
}

void * allocateAligned(std::size_t size, std::size_t alignment)
{
  record(size);
#if defined(_WIN32)
  if (void * pointer = _aligned_malloc(size == 0 ? alignment : size, alignment)) {
    return pointer;
  }
#else
  void * pointer = nullptr;
  if (posix_memalign(&pointer, alignment, size == 0 ? alignment : size) == 0) {
    return pointer;
  }
#endif
  throw std::bad_alloc();
}

void deallocateAligned(void * pointer) noexcept
{
#if defined(_WIN32)
  _aligned_free(pointer);
#else
  std::free(pointer);
#endif
}

} // namespace allocation_counter

void * operator new(std::size_t size)
{
  return allocation_counter::allocate(size);
}

void * operator new[](std::size_t size)
{
  return allocation_counter::allocate(size);
}

void * operator new(std::size_t size, const std::nothrow_t&) noexcept
{
  try {
    return allocation_counter::allocate(size);
  } catch (...) {
    return nullptr;
  }
}

void * operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
  try {
    return allocation_counter::allocate(size);
  } catch (...) {
    return nullptr;
  }
}

void * operator new(std::size_t size, std::align_val_t alignment)
{
  return allocation_counter::allocateAligned(size, static_cast<std::size_t>(alignment));
}

void * operator new[](std::size_t size, std::align_val_t alignment)
{
  return allocation_counter::allocateAligned(size, static_cast<std::size_t>(alignment));
}

void * operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
  try {
    return allocation_counter::allocateAligned(size, static_cast<std::size_t>(alignment));
  } catch (...) {
    return nullptr;
  }
}

void * operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
  try {
    return allocation_counter::allocateAligned(size, static_cast<std::size_t>(alignment));
  } catch (...) {
    return nullptr;
  }
}

void operator delete(void * pointer) noexcept
{
  std::free(pointer);
}

void operator delete[](void * pointer) noexcept
{
  std::free(pointer);
}

void operator delete(void * pointer, std::size_t) noexcept
{
  std::free(pointer);
}

void operator delete[](void * pointer, std::size_t) noexcept
{
  std::free(pointer);
}

void operator delete(void * pointer, const std::nothrow_t&) noexcept
{
  std::free(pointer);
}

void operator delete[](void * pointer, const std::nothrow_t&) noexcept
{
  std::free(pointer);
}

void operator delete(void * pointer, std::align_val_t) noexcept
{
  allocation_counter::deallocateAligned(pointer);
}

void operator delete[](void * pointer, std::align_val_t) noexcept
{
  allocation_counter::deallocateAligned(pointer);
}

void operator delete(void * pointer, std::size_t, std::align_val_t) noexcept
{
  allocation_counter::deallocateAligned(pointer);
}

void operator delete[](void * pointer, std::size_t, std::align_val_t) noexcept
{
  allocation_counter::deallocateAligned(pointer);
}

void operator delete(void * pointer, std::align_val_t, const std::nothrow_t&) noexcept
{
  allocation_counter::deallocateAligned(pointer);
}

void operator delete[](void * pointer, std::align_val_t, const std::nothrow_t&) noexcept
{
  allocation_counter::deallocateAligned(pointer);
}

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t sampleCount = 101;
constexpr std::size_t warmupCount = 10;

volatile std::size_t resultSink = 0;

struct Workload {
    const char * name;
    std::array<std::string, 4> sources;
    std::array<std::vector<std::uint8_t>, 4> encoded;
    std::size_t sourceCount = 0;
    std::size_t sourceBytes = 0;
    std::size_t astBytes = 0;
#if defined(MUSTACHE_CISTA_BENCHMARK)
    std::vector<std::uint8_t> cistaEncoded;
#endif
};

struct Sample {
    double nanoseconds;
    std::uint64_t allocations;
    std::uint64_t bytes;
};

struct Result {
    double medianMicroseconds;
    double p95Microseconds;
    std::uint64_t medianAllocations;
    std::uint64_t medianBytes;
};

std::size_t percentileIndex(std::size_t size, double fraction)
{
  return std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(static_cast<double>(size) * fraction))) - 1;
}

template <typename Operation> Result measure(Operation operation)
{
  for (std::size_t i = 0; i < warmupCount; ++i) {
    resultSink = resultSink ^ operation();
  }

  std::vector<Sample> samples;
  samples.reserve(sampleCount);
  for (std::size_t i = 0; i < sampleCount; ++i) {
    allocation_counter::allocations = 0;
    allocation_counter::bytes = 0;
    const Clock::time_point start = Clock::now();
    allocation_counter::enabled = true;
    const std::size_t value = operation();
    allocation_counter::enabled = false;
    const Clock::time_point stop = Clock::now();
    resultSink = resultSink ^ value;
    samples.push_back({
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count()),
        allocation_counter::allocations,
        allocation_counter::bytes,
    });
  }

  std::vector<double> times;
  std::vector<std::uint64_t> allocations;
  std::vector<std::uint64_t> bytes;
  times.reserve(samples.size());
  allocations.reserve(samples.size());
  bytes.reserve(samples.size());
  for (const Sample& sample : samples) {
    times.push_back(sample.nanoseconds);
    allocations.push_back(sample.allocations);
    bytes.push_back(sample.bytes);
  }
  std::sort(times.begin(), times.end());
  std::sort(allocations.begin(), allocations.end());
  std::sort(bytes.begin(), bytes.end());

  return {
      times[percentileIndex(times.size(), 0.50)] / 1000.0,
      times[percentileIndex(times.size(), 0.95)] / 1000.0,
      allocations[percentileIndex(allocations.size(), 0.50)],
      bytes[percentileIndex(bytes.size(), 0.50)],
  };
}

#if defined(MUSTACHE_CISTA_BENCHMARK)
std::vector<std::uint8_t> compileCistaGraph(mustache::Mustache& engine, const Workload& workload)
{
  mustache::Node root;
  engine.tokenize(workload.sources[0], &root);
  mustache::Node::Partials partials;
  if (workload.sourceCount > 1) {
    const std::array<const char *, 3> names = {"layout", "card", "badge"};
    for (std::size_t index = 1; index < workload.sourceCount; ++index) {
      std::unique_ptr<mustache::Node> partial = std::make_unique<mustache::Node>();
      engine.tokenize(workload.sources[index], partial.get());
      partials.emplace(names[index - 1], std::move(partial));
    }
  }
  return mustache_benchmark::serializeCistaArchive(root, partials);
}
#endif

Workload makeWorkload(const char * name, std::size_t targetBytes, bool nestedPartials)
{
  const std::string graphRoot =
      "<!doctype html>\n<html>\n<body>\n{{#products}}\n  {{> layout}}\n{{/products}}\n</body>\n</html>\n";
  const std::string layout = "<main aria-label=\"{{title}}\">\n  {{> card}}\n</main>\n";
  const std::string badge = "{{#featured}}<strong class=\"badge\">Featured</strong>{{/featured}}\n";
  const std::string unit = "{{! production-shaped product card }}\n"
                           "<section class=\"product product--{{category.slug}}\">\n"
                           "  <h2>{{title}}</h2>\n"
                           "  {{#available}}\n"
                           "    <p>{{description}}</p>\n"
                           "    <span data-id=\"{{id}}\">{{price}}</span>\n"
                           "  {{/available}}\n"
                           "  {{^available}}\n"
                           "    <span class=\"unavailable\">Unavailable</span>\n"
                           "  {{/available}}\n"
                           "  {{> badge}}\n"
                           "</section>\n";

  std::array<std::string, 4> sources;
  std::size_t sourceCount = 0;
  std::string card;
  if (nestedPartials) {
    while (graphRoot.size() + layout.size() + badge.size() + card.size() < targetBytes) {
      card.append(unit);
    }
    sources = {graphRoot, layout, card, badge};
    sourceCount = sources.size();
  } else {
    const std::string prefix = "<!doctype html>\n<html>\n<body>\n{{#products}}\n";
    const std::string suffix = "{{/products}}\n</body>\n</html>\n";
    const std::string flatUnit = "{{! production-shaped product card }}\n"
                                 "<section class=\"product product--{{category.slug}}\">\n"
                                 "  <h2>{{title}}</h2>\n"
                                 "  {{#available}}\n"
                                 "    <p>{{description}}</p>\n"
                                 "    <span data-id=\"{{id}}\">{{price}}</span>\n"
                                 "  {{/available}}\n"
                                 "  {{^available}}\n"
                                 "    <span class=\"unavailable\">Unavailable</span>\n"
                                 "  {{/available}}\n"
                                 "  {{#featured}}<strong class=\"badge\">Featured</strong>{{/featured}}\n"
                                 "</section>\n";
    while (prefix.size() + card.size() + suffix.size() < targetBytes) {
      card.append(flatUnit);
    }
    sources = {prefix + card + suffix, std::string(), std::string(), std::string()};
    sourceCount = 1;
  }

  Workload workload{};
  workload.name = name;
  workload.sources = std::move(sources);
  workload.sourceCount = sourceCount;
  mustache::Mustache engine;
  for (std::size_t i = 0; i < workload.sourceCount; ++i) {
    workload.sourceBytes += workload.sources[i].size();
    mustache::Node parsed;
    engine.tokenize(workload.sources[i], &parsed);
    workload.encoded[i] = parsed.serializeValue();
    workload.astBytes += workload.encoded[i].size();
  }
#if defined(MUSTACHE_CISTA_BENCHMARK)
  workload.cistaEncoded = compileCistaGraph(engine, workload);
#endif
  return workload;
}

std::string_view byteView(const std::vector<std::uint8_t>& value)
{
  return std::string_view(reinterpret_cast<const char *>(value.data()), value.size());
}

mustache::Data makeData()
{
  mustache::Data category = mustache::Data::object();
  category.set("slug", mustache::Data::string("libraries"));

  mustache::Data product = mustache::Data::object();
  product.set("id", mustache::Data::string("product-123"));
  product.set("title", mustache::Data::string("A rock-hard template engine"));
  product.set("description", mustache::Data::string("Safe ownership, bounded parsing, and compatible rendering."));
  product.set("price", mustache::Data::string("$19.50"));
  product.set("available", mustache::Data::boolean(true));
  product.set("featured", mustache::Data::boolean(true));
  product.set("category", std::move(category));

  mustache::Data products = mustache::Data::array();
  products.push_back(std::move(product));
  mustache::Data root = mustache::Data::object();
  root.set("products", std::move(products));
  return root;
}

void verify(const Workload& workload)
{
  mustache::Mustache engine;
  const mustache::Data data = makeData();

  const mustache::CompiledTemplate compiledRoot = engine.compile(workload.sources[0]);
  mustache::PartialMap compiledPartials;
  if (workload.sourceCount > 1) {
    compiledPartials.emplace("layout", engine.compile(workload.sources[1]));
    compiledPartials.emplace("card", engine.compile(workload.sources[2]));
    compiledPartials.emplace("badge", engine.compile(workload.sources[3]));
  }
  const std::string compiledOutput = engine.render(compiledRoot, data, compiledPartials);

  std::unique_ptr<mustache::Node> decodedRoot = mustache::Node::unserializeOwned(byteView(workload.encoded[0]));
  mustache::Node::Partials decodedPartials;
  if (workload.sourceCount > 1) {
    decodedPartials.emplace("layout", mustache::Node::unserializeOwned(byteView(workload.encoded[1])));
    decodedPartials.emplace("card", mustache::Node::unserializeOwned(byteView(workload.encoded[2])));
    decodedPartials.emplace("badge", mustache::Node::unserializeOwned(byteView(workload.encoded[3])));
  }
  std::string decodedOutput;
  engine.render(decodedRoot.get(), &data, &decodedPartials, &decodedOutput);

  if (compiledOutput.empty() || compiledOutput != decodedOutput) {
    throw std::runtime_error(std::string("source and decoded rendering differ for ") + workload.name);
  }
#if defined(MUSTACHE_CISTA_BENCHMARK)
  const std::string cistaOutput = mustache_benchmark::renderCistaArchive(byteView(workload.cistaEncoded), data);
  if (compiledOutput != cistaOutput) {
    throw std::runtime_error(std::string("source and Cista rendering differ for ") + workload.name);
  }
#endif
  for (std::size_t i = 0; i < workload.sourceCount; ++i) {
    const std::unique_ptr<mustache::Node> decoded = mustache::Node::unserializeOwned(byteView(workload.encoded[i]));
    if (decoded->serializeValue() != workload.encoded[i]) {
      throw std::runtime_error(std::string("AST bytes do not round-trip for ") + workload.name);
    }
  }
}

void printResult(const Workload& workload, const char * operation, const Result& result)
{
#if defined(MUSTACHE_CISTA_BENCHMARK)
  std::printf("%s,%zu,%zu,%zu,%s,%.3f,%.3f,%llu,%llu\n", workload.name, workload.sourceBytes, workload.astBytes,
      workload.cistaEncoded.size(), operation, result.medianMicroseconds, result.p95Microseconds,
      static_cast<unsigned long long>(result.medianAllocations), static_cast<unsigned long long>(result.medianBytes));
#else
  std::printf("%s,%zu,%zu,%s,%.3f,%.3f,%llu,%llu\n", workload.name, workload.sourceBytes, workload.astBytes, operation,
      result.medianMicroseconds, result.p95Microseconds, static_cast<unsigned long long>(result.medianAllocations),
      static_cast<unsigned long long>(result.medianBytes));
#endif
}

} // namespace

int main()
{
  try {
    std::array<Workload, 6> workloads = {
        makeWorkload("small-flat", 1024, false),
        makeWorkload("small-graph", 1024, true),
        makeWorkload("medium-flat", 32768, false),
        makeWorkload("medium-graph", 32768, true),
        makeWorkload("large-flat", 262144, false),
        makeWorkload("large-graph", 262144, true),
    };

#if defined(MUSTACHE_CISTA_BENCHMARK)
    std::puts("size,source_bytes,ast_bytes,cista_bytes,operation,median_us,p95_us,allocations,allocated_bytes");
#else
    std::puts("size,source_bytes,ast_bytes,operation,median_us,p95_us,allocations,allocated_bytes");
#endif
    for (const Workload& workload : workloads) {
      verify(workload);
      mustache::Mustache engine;

      const Result compile = measure([&engine, &workload]() -> std::size_t {
        std::array<mustache::CompiledTemplate, 4> compiled;
        std::size_t digest = 0;
        for (std::size_t i = 0; i < workload.sourceCount; ++i) {
          compiled[i] = engine.compile(workload.sources[i]);
          digest += compiled[i].empty() ? 0 : 1;
        }
        return digest;
      });
      printResult(workload, "compile_source_graph", compile);

      const Result decode = measure([&workload]() -> std::size_t {
        std::array<std::unique_ptr<mustache::Node>, 4> decoded;
        std::size_t digest = 0;
        for (std::size_t i = 0; i < workload.sourceCount; ++i) {
          decoded[i] = mustache::Node::unserializeOwned(byteView(workload.encoded[i]));
          digest += static_cast<std::size_t>(decoded[i]->type);
        }
        return digest;
      });
      printResult(workload, "decode_ast_graph", decode);

#if defined(MUSTACHE_CISTA_BENCHMARK)
      const Result compileAndSerializeCista = measure([&engine, &workload]() -> std::size_t {
        return compileCistaGraph(engine, workload).size();
      });
      printResult(workload, "compile_serialize_cista_graph", compileAndSerializeCista);
#endif

      const mustache::Data data = makeData();
      const mustache::CompiledTemplate residentRoot = engine.compile(workload.sources[0]);
      mustache::PartialMap residentPartials;
      if (workload.sourceCount > 1) {
        residentPartials.emplace("layout", engine.compile(workload.sources[1]));
        residentPartials.emplace("card", engine.compile(workload.sources[2]));
        residentPartials.emplace("badge", engine.compile(workload.sources[3]));
      }
      const Result residentRender = measure([&engine, &residentRoot, &residentPartials, &data]() -> std::size_t {
        return engine.render(residentRoot, data, residentPartials).size();
      });
      printResult(workload, "reuse_compiled_render", residentRender);

      const Result compileAndRender = measure([&engine, &workload, &data]() -> std::size_t {
        const mustache::CompiledTemplate root = engine.compile(workload.sources[0]);
        mustache::PartialMap partials;
        if (workload.sourceCount > 1) {
          partials.emplace("layout", engine.compile(workload.sources[1]));
          partials.emplace("card", engine.compile(workload.sources[2]));
          partials.emplace("badge", engine.compile(workload.sources[3]));
        }
        return engine.render(root, data, partials).size();
      });
      printResult(workload, "compile_render_source_graph", compileAndRender);

      const Result decodeAndRender = measure([&workload, &data]() -> std::size_t {
        std::unique_ptr<mustache::Node> root = mustache::Node::unserializeOwned(byteView(workload.encoded[0]));
        mustache::Node::Partials partials;
        if (workload.sourceCount > 1) {
          partials.emplace("layout", mustache::Node::unserializeOwned(byteView(workload.encoded[1])));
          partials.emplace("card", mustache::Node::unserializeOwned(byteView(workload.encoded[2])));
          partials.emplace("badge", mustache::Node::unserializeOwned(byteView(workload.encoded[3])));
        }
        mustache::Mustache engine;
        std::string output;
        engine.render(root.get(), &data, &partials, &output);
        return output.size();
      });
      printResult(workload, "decode_render_ast_graph", decodeAndRender);

#if defined(MUSTACHE_CISTA_BENCHMARK)
      const Result validateAndRenderCista = measure([&workload, &data]() -> std::size_t {
        return mustache_benchmark::renderCistaArchive(byteView(workload.cistaEncoded), data).size();
      });
      printResult(workload, "validate_render_cista_graph", validateAndRenderCista);
#endif
    }
  } catch (const std::exception& exception) {
    std::fprintf(stderr, "benchmark failed: %s\n", exception.what());
    return 1;
  }

  return resultSink == static_cast<std::size_t>(-1) ? 2 : 0;
}
