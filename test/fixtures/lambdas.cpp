
#include <iostream>
#include <sstream>

#include "data.hpp"

#include "lambdas.hpp"

static void add_lambda(mustache::Lambda * lambda, mustache::Data * data)
{
	mustache::Data * child = new mustache::Data();
	child->type = mustache::Data::TypeLambda;
	child->lambda = lambda;
	if( data->type == mustache::Data::TypeMap ) {
		data->data.erase("lambda");
		data->data.insert(std::pair<std::string,mustache::Data *>("lambda", child));
	} else {
		std::cerr << "Root data was not a map!" << std::endl;
		delete child;
	}
}

void load_lambdas_into_test_data(mustache::Data * data, std::string name)
{
	if( name.compare("Interpolation") == 0 ) {
		add_lambda(new StaticLambda("world"), data);
	} else if( name.compare("Interpolation - Expansion") == 0 ) {
		add_lambda(new StaticLambda("{{planet}}"), data);
	} else if( name.compare("Interpolation - Alternate Delimiters") == 0 ) {
		add_lambda(new StaticLambda("|planet| => {{planet}}"), data);
	} else if( name.compare("Interpolation - Multiple Calls") == 0 ) {
		add_lambda(new MultipleCallsLambda(), data);
	} else if( name.compare("Escaping") == 0 ) {
		add_lambda(new StaticLambda(">"), data);
	} else if( name.compare("Section") == 0 ) {
		add_lambda(new SectionLambda(), data);
	} else if( name.compare("Section - Expansion") == 0 ) {
		add_lambda(new SectionExpansionLambda(), data);
	} else if( name.compare("Section - Alternate Delimiters") == 0 ) {
		add_lambda(new SectionAlternateDelimitersLambda(), data);
	} else if( name.compare("Section - Multiple Calls") == 0 ) {
		add_lambda(new SectionMultipleCallsLambda(), data);
	} else if( name.compare("Inverted Section") == 0 ) {
		add_lambda(new StaticLambda(""), data);
	}
}



std::string MultipleCallsLambda::invoke() {
	std::ostringstream st;
	st << std::dec << ++this->counter;
	return st.str();
}

std::string MultipleCallsLambda::invoke(std::string * text, mustache::Renderer * renderer) {
	return invoke();
}



std::string SectionLambda::invoke() {
	throw new mustache::Exception("This is a section lambda");
}

std::string SectionLambda::invoke(std::string * text, mustache::Renderer * renderer) {
	if( text->compare("{{x}}") == 0 ) {
		return "yes";
	} else {
		return "no";
	}
}




std::string SectionExpansionLambda::invoke() {
	throw new mustache::Exception("This is a section lambda");
}

std::string SectionExpansionLambda::invoke(std::string * text, mustache::Renderer * renderer) {
	std::ostringstream st;
	st << *text << "{{planet}}" << *text;
	return st.str();
}




std::string SectionAlternateDelimitersLambda::invoke() {
	throw new mustache::Exception("This is a section lambda");
}

std::string SectionAlternateDelimitersLambda::invoke(std::string * text, mustache::Renderer * renderer) {
	std::ostringstream st;
	st << *text << "{{planet}} => |planet|" << *text;
	return st.str();
}




std::string SectionMultipleCallsLambda::invoke() {
	throw new mustache::Exception("This is a section lambda");
}

std::string SectionMultipleCallsLambda::invoke(std::string * text, mustache::Renderer * renderer) {
	std::ostringstream st;
	st << "__";
	st << *text;
	st << "__";
	return st.str();
}

