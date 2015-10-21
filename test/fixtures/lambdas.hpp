
#ifndef MUSTACHE_TEST_FIXTURES_LAMBDAS_HPP
#define MUSTACHE_TEST_FIXTURES_LAMBDAS_HPP

#include "renderer.hpp"
#include "lambda.hpp"

void load_lambdas_into_test_data(mustache::Data * data, std::string name);

class StaticLambda: public mustache::Lambda {
private:
	std::string tmpl;
public:
	StaticLambda(std::string tmpl) {
		this->tmpl = tmpl;
	}

    std::string invoke() {
    	return this->tmpl;
    }

    std::string invoke(std::string * text, mustache::Renderer * renderer) {
    	return this->tmpl;
    }
};

class MultipleCallsLambda: public mustache::Lambda {
private:
	int counter;
public:
	MultipleCallsLambda(): counter(0) {}
    std::string invoke();
    std::string invoke(std::string * text, mustache::Renderer * renderer);
};

class SectionLambda: public mustache::Lambda {
    std::string invoke();
    std::string invoke(std::string * text, mustache::Renderer * renderer);
};

class SectionExpansionLambda: public mustache::Lambda {
    std::string invoke();
    std::string invoke(std::string * text, mustache::Renderer * renderer);
};

class SectionAlternateDelimitersLambda: public mustache::Lambda {
    std::string invoke();
    std::string invoke(std::string * text, mustache::Renderer * renderer);
};

class SectionMultipleCallsLambda: public mustache::Lambda {
public:
    std::string invoke();
    std::string invoke(std::string * text, mustache::Renderer * renderer);
};

#endif
