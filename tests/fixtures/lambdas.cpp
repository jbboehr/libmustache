
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>

#include "data.hpp"

#include "lambdas.hpp"

static void add_lambda(std::unique_ptr<mustache::Lambda> lambda, mustache::Data * data)
{
  if (data->type() == mustache::Data::TypeMap) {
    data->set("lambda", mustache::Data::lambda(std::move(lambda)));
  } else {
    std::cerr << "Root data was not a map!" << std::endl;
  }
}

void load_lambdas_into_test_data(mustache::Data * data, std::string name)
{
  if (name.compare("Interpolation") == 0) {
    add_lambda(std::make_unique<StaticLambda>("world"), data);
  } else if (name.compare("Interpolation - Expansion") == 0) {
    add_lambda(std::make_unique<StaticLambda>("{{planet}}"), data);
  } else if (name.compare("Interpolation - Alternate Delimiters") == 0) {
    add_lambda(std::make_unique<StaticLambda>("|planet| => {{planet}}"), data);
  } else if (name.compare("Interpolation - Multiple Calls") == 0) {
    add_lambda(std::make_unique<MultipleCallsLambda>(), data);
  } else if (name.compare("Escaping") == 0) {
    add_lambda(std::make_unique<StaticLambda>(">"), data);
  } else if (name.compare("Section") == 0) {
    add_lambda(std::make_unique<SectionLambda>(), data);
  } else if (name.compare("Section - Expansion") == 0) {
    add_lambda(std::make_unique<SectionExpansionLambda>(), data);
  } else if (name.compare("Section - Alternate Delimiters") == 0) {
    add_lambda(std::make_unique<SectionAlternateDelimitersLambda>(), data);
  } else if (name.compare("Section - Multiple Calls") == 0) {
    add_lambda(std::make_unique<SectionMultipleCallsLambda>(), data);
  } else if (name.compare("Inverted Section") == 0) {
    add_lambda(std::make_unique<StaticLambda>(""), data);
  }
}

std::string MultipleCallsLambda::invoke()
{
  std::ostringstream st;
  st << std::dec << ++this->counter;
  return st.str();
}

std::string MultipleCallsLambda::invoke(std::string *, mustache::Renderer *)
{
  return invoke();
}

std::string SectionLambda::invoke()
{
  throw mustache::Exception("This is a section lambda");
}

std::string SectionLambda::invoke(std::string * text, mustache::Renderer *)
{
  if (text->compare("{{x}}") == 0) {
    return "yes";
  } else {
    return "no";
  }
}

std::string SectionExpansionLambda::invoke()
{
  throw mustache::Exception("This is a section lambda");
}

std::string SectionExpansionLambda::invoke(std::string * text, mustache::Renderer *)
{
  std::ostringstream st;
  st << *text << "{{planet}}" << *text;
  return st.str();
}

std::string SectionAlternateDelimitersLambda::invoke()
{
  throw mustache::Exception("This is a section lambda");
}

std::string SectionAlternateDelimitersLambda::invoke(std::string * text, mustache::Renderer *)
{
  std::ostringstream st;
  st << *text << "{{planet}} => |planet|" << *text;
  return st.str();
}

std::string SectionMultipleCallsLambda::invoke()
{
  throw mustache::Exception("This is a section lambda");
}

std::string SectionMultipleCallsLambda::invoke(std::string * text, mustache::Renderer *)
{
  std::ostringstream st;
  st << "__";
  st << *text;
  st << "__";
  return st.str();
}
