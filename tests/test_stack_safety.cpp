#include "mustache_config.h"

#include <cstdio>
#include <limits>

#include "exception.hpp"
#include "stack.hpp"

#if defined(MUSTACHE_TEST_STACK_EXPECT_UNCHECKED) && !defined(MUSTACHE_STACK_UNCHECKED)
#error "unchecked stack safety test must disable bounds checks"
#endif

namespace {

int failures = 0;

void expect(bool condition, const char * message)
{
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
}

void expectAt(bool condition, const char * message, int size, int offset)
{
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s (size=%d, offset=%d)\n", message, size, offset);
    ++failures;
  }
}

#ifndef MUSTACHE_STACK_UNCHECKED
bool rejectsBackOffset(mustache::Stack<int, 3>& stack, int offset)
{
  try {
    static_cast<void>(stack.backOffset(offset));
  } catch (const mustache::Exception&) {
    return true;
  }
  return false;
}
#endif

void verifyState(mustache::Stack<int, 3>& stack, const int * expected, int size)
{
  expect(stack.size() == size, "size must match the reference state");
  const bool endpointMatches = size == 0 ? stack.end() == stack.begin() : stack.end() == stack.begin() + size - 1;
  expect(endpointMatches, "end must preserve the legacy top-pointer contract");

  if (endpointMatches && size > 0) {
    int * position = stack.end();
    for (int offset = 0; offset < size; ++offset) {
      expectAt(
          *position == expected[size - 1 - offset], "legacy traversal must preserve top-to-bottom order", size, offset);
      if (offset + 1 < size) {
        --position;
      }
    }
  }

  for (int offset = 0; offset < size; ++offset) {
    expectAt(stack.backOffset(offset) == expected[size - 1 - offset], "valid offsets must remain top-relative", size,
        offset);
  }

#ifndef MUSTACHE_STACK_UNCHECKED
  for (int offset = -8; offset < 0; ++offset) {
    expectAt(rejectsBackOffset(stack, offset), "negative offsets must be rejected", size, offset);
  }
  expectAt(rejectsBackOffset(stack, std::numeric_limits<int>::min()), "the minimum integer offset must be rejected",
      size, std::numeric_limits<int>::min());

  for (int offset = size; offset <= size + 4; ++offset) {
    expectAt(rejectsBackOffset(stack, offset), "offsets at or above the size must be rejected", size, offset);
  }
  expectAt(rejectsBackOffset(stack, std::numeric_limits<int>::max()), "the maximum integer offset must be rejected",
      size, std::numeric_limits<int>::max());
#endif
}

void testStateTransitions()
{
  mustache::Stack<int, 3> stack;
  const int expected[] = {17, 23, 41};

  verifyState(stack, expected, 0);
  for (int size = 1; size <= 3; ++size) {
    stack.push_back(expected[size - 1]);
    verifyState(stack, expected, size);
  }

  expect(stack.pop_back() == 41, "pop must return the former top element");
  verifyState(stack, expected, 2);

  stack.clear();
  verifyState(stack, expected, 0);

  const int reused[] = {53};
  stack.push_back(reused[0]);
  verifyState(stack, reused, 1);
}

#ifdef MUSTACHE_STACK_UNCHECKED
void testUncheckedOptOut()
{
  mustache::Stack<int, 3> stack;
  stack.push_back(71);
  stack.push_back(89);
  expect(stack.pop_back() == 89, "unchecked setup must leave one current element");

  bool threw = false;
  try {
    // For this state, offset -1 addresses initialized backing storage while
    // remaining outside the logical stack, so it safely distinguishes whether
    // MUSTACHE_STACK_UNCHECKED still removes the bounds exception.
    static_cast<void>(stack.backOffset(-1));
  } catch (const mustache::Exception&) {
    threw = true;
  }
  expect(!threw, "MUSTACHE_STACK_UNCHECKED must preserve the bounds-check opt-out");
}
#endif

} // namespace

int main()
{
  testStateTransitions();
#ifdef MUSTACHE_STACK_UNCHECKED
  testUncheckedOptOut();
#endif
  return failures;
}
