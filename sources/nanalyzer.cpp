#include "nanalyzer.hpp"

#include <fmt/core.h>

#include <exception>
#include <stdexcept>

#include "constants.hpp"

int NaNalyzer::run(){
	fmt::println("{}", Constants::Title);

	try{
		parseCsv();
		defineInvalidData();
		defineColumnCombinations();
		// process();
	}catch(const std::exception &error){
		fmt::print(stderr, "Fatal error: {}\n", error.what());
		return 1;
	}

	return 0;
}