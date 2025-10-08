#include "nanalyzer.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <cctype>
#include <exception>
#include <stdexcept>
#include <algorithm>

#include "constants.hpp"

int NaNalyzer::run(){
	fmt::println("{}", Constants::Title);

	try{
		parseCsv();

		if(columns_.empty()) defineInvalidData();
		if(columnCombinationsToCheck_.empty()) defineColumnCombinations();	

		auto parseYesNo{[](const std::string &input, const bool defaultValue){
			if(input.empty()){
				return defaultValue;
			}

			const char firstCharacter{static_cast<char>(std::tolower(static_cast<unsigned char>(input.front())))};
			if(firstCharacter == 'y'){
				return true;
			}
			if(firstCharacter == 'n'){
				return false;
			}

			return defaultValue;
		}};

		bool shouldProcess{true};

		if(configurationLoadedFromJson_){
			fmt::print("\nProceed to processing the CSV now? (Y/n): ");
			std::string response{};
			std::getline(std::cin, response);
			shouldProcess = parseYesNo(response, true);
		}else{
			fmt::print("\nWould you like to save these initialization settings to a JSON file? (y/N): ");
			std::string saveResponse;
			std::getline(std::cin, saveResponse);
			const bool wantsToSave{parseYesNo(saveResponse, false)};

			if(wantsToSave){
				const std::time_t currentTime{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
				const std::tm localTime{*std::localtime(&currentTime)};
				const std::string defaultFileName{fmt::format(
					"{}_{:%Y%m%d_%H%M%S}.json",
					Constants::DefaultBaseJsonName,
					localTime
				)};

				fmt::print("Enter the file name to save [{}]: ", defaultFileName);
				std::string fileNameInput;
				std::getline(std::cin, fileNameInput);
				std::string targetFileName{fileNameInput.empty() ? defaultFileName : fileNameInput};
				if(!targetFileName.empty()){
					std::string lowerCaseName{targetFileName};
					std::transform(
						lowerCaseName.begin(), lowerCaseName.end(), lowerCaseName.begin(),
						[](unsigned char character){ return static_cast<char>(std::tolower(character)); }
					);

					if(!lowerCaseName.ends_with(".json")){
						targetFileName.append(".json");
					}
				}

				try{
					saveInitializationToJson(targetFileName);
					fmt::println("Saved initialization settings to '{}'.", targetFileName);
				}catch(const std::exception &exception){
					fmt::println(stderr, "Failed to save initialization settings: {}", exception.what());
				}

				fmt::print("\nProceed to processing the CSV now? (Y/n): ");
				std::string processResponse;
				std::getline(std::cin, processResponse);
				shouldProcess = parseYesNo(processResponse, true);
			}
		}

		if(shouldProcess){
			process();
		}else{
			fmt::println("Skipping processing per user request.");
		}

	}catch(const std::exception &error){
		fmt::println(stderr, "Fatal error: {}", error.what());
		return 1;
	}

	return 0;
}