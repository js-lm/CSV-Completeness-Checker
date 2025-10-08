#include "nanalyzer.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <csv.h>

void NaNalyzer::parseCsv(){
	fmt::print(
		"Enter the path to the source CSV file or an initialization JSON file "
		"(e.g., data.csv or session.json): "
	);
	std::string userInput;
	std::cin >> userInput;

	configurationLoadedFromJson_ = false;

	std::string lowerCasedInput{userInput};
	std::transform(
		lowerCasedInput.begin(), lowerCasedInput.end(), lowerCasedInput.begin(),
		[](unsigned char character){ return static_cast<char>(std::tolower(character));}
	);

	const bool isJsonInput{lowerCasedInput.ends_with(".json")};

	if(isJsonInput){
		try{
			loadInitializationFromJson(userInput);
			fmt::println("Loaded initialization settings from '{}'.", userInput);
		}catch(const std::exception &exception){
			throw std::runtime_error{fmt::format(
				"Failed to load initialization from '{}'. {}",
				userInput,
				exception.what()
			)};
		}
	}else{
		csvFilePath_ = std::move(userInput);

		io::LineReader csvLineReader{csvFilePath_};
		try{
			char *headerLine{csvLineReader.next_line()};
			if(!headerLine){
				throw std::runtime_error{"No header line found in CSV file."};
			}
			headers_ = splitString(std::string{headerLine}, ',');
		}catch(const std::exception &exception){
			throw std::runtime_error{fmt::format(
				"Could not open or read file '{}'. {}",
				csvFilePath_,
				exception.what()
			)};
		}
	}

	clearInputBuffer();

	if(headers_.empty()){
		throw std::runtime_error{"No header line found in CSV file."};
	}

	fmt::println("Source CSV file: {}", csvFilePath_);

	fmt::println("\n--- Discovered Fields ---");
	for(std::size_t headerIndex{0}; headerIndex < headers_.size(); headerIndex++){
		fmt::println("[{}] {}", headerIndex + 1, headers_[headerIndex]);
	}

	if(configurationLoadedFromJson_ && !columns_.empty()){
		fmt::println("\n--- Fields loaded from configuration ---");
		std::vector<int> sortedColumnIdentifiers;
		sortedColumnIdentifiers.reserve(columns_.size());

		for(const auto &columnEntry : columns_){
			sortedColumnIdentifiers.push_back(columnEntry.first);
		}

		std::sort(sortedColumnIdentifiers.begin(), sortedColumnIdentifiers.end());

		for(const int columnIdentifier : sortedColumnIdentifiers){
			const Column &columnDefinition{columns_.at(columnIdentifier)};
			fmt::println("[{}] {}", columnIdentifier, columnDefinition.name);
		}
	}

	if(configurationLoadedFromJson_ && !columnCombinationsToCheck_.empty()){
		fmt::println("\n--- Field combinations loaded from configuration ---");
		for(const auto &combination : columnCombinationsToCheck_){
			std::vector<int> displayIndices;
			displayIndices.reserve(combination.size());

			for(const ColumnOffset columnOffset : combination){
				displayIndices.push_back(columnOffset + 1);
			}

			fmt::println("[{}]", fmt::join(displayIndices, ":"));
		}
	}
}

void NaNalyzer::defineInvalidData(){
	if(configurationLoadedFromJson_ && !columns_.empty()){
		fmt::println("\n--- Fields loaded from configuration ---");
		std::vector<int> sortedColumnIdentifiers;
		sortedColumnIdentifiers.reserve(columns_.size());

		for(const auto &columnEntry : columns_){
			sortedColumnIdentifiers.push_back(columnEntry.first);
		}

		std::sort(sortedColumnIdentifiers.begin(), sortedColumnIdentifiers.end());

		for(const int columnIdentifier : sortedColumnIdentifiers){
			const Column &columnDefinition{columns_.at(columnIdentifier)};
			fmt::println("[{}] {}", columnIdentifier, columnDefinition.name);
		}
		return;
	}

	if(headers_.empty()){
		throw std::runtime_error{"No headers were discovered. Cannot define invalid data."};
	}

	columns_.clear();

	fmt::println("\n--- Select Fields to Analyze ---");
	fmt::println("Enter a field number to add it. Type 'done' when finished.");

	while(true){
		fmt::print("Field number to add (or 'done'): ");
		std::string userInput;
		std::cin >> userInput;

		if(userInput == "done"){
			if(columns_.empty()){
				throw std::runtime_error{"No fields were selected. Exiting..."};
			}
			break;
		}

		try{
			int fieldNumber{std::stoi(userInput)};
			if(fieldNumber < 1 || fieldNumber > static_cast<int>(headers_.size())){
				fmt::println(stderr, "Invalid number. Please enter a number between 1 and {}.", headers_.size());
				continue;
			}

			if(columns_.contains(fieldNumber)){
				fmt::println("Field {} is already selected.", fieldNumber);
				continue;
			}

			fmt::print(" -> For field '{}', list any values that count as empty (comma separated, or press Enter for none): ", headers_[fieldNumber - 1]);
			clearInputBuffer();
			std::string invalidValuesInput;
			std::getline(std::cin, invalidValuesInput);

			Column columnDefinition;
			columnDefinition.index = fieldNumber - 1;
			columnDefinition.name = headers_[fieldNumber - 1];

			DelimitedStringList invalidValuesList{splitString(invalidValuesInput, ',')};
			columnDefinition.invalidValues = InvalidValueSet{invalidValuesList.begin(), invalidValuesList.end()};

			columns_.emplace(fieldNumber, std::move(columnDefinition));
			fmt::println("Added field: {}", headers_[fieldNumber - 1]);
		}catch(const std::invalid_argument &){
			fmt::println(stderr, "Invalid input. Please enter a number or 'done'.");
		}catch(const std::out_of_range &){
			fmt::println(stderr, "Invalid number. Please try again.");
		}
	}

	fmt::println("\n--- You selected the following fields ---");
	std::vector<int> sortedColumnIdentifiers;
	sortedColumnIdentifiers.reserve(columns_.size());

	for(const auto &columnEntry : columns_){
		sortedColumnIdentifiers.push_back(columnEntry.first);
	}

	std::sort(sortedColumnIdentifiers.begin(), sortedColumnIdentifiers.end());

	for(const int columnIdentifier : sortedColumnIdentifiers){
		const Column &columnDefinition{columns_.at(columnIdentifier)};
		fmt::println("[{}] {}", columnIdentifier, columnDefinition.name);
	}
}

void NaNalyzer::defineColumnCombinations(){
	if(configurationLoadedFromJson_ && !columnCombinationsToCheck_.empty()){
		fmt::println("\n--- Field combinations loaded from configuration ---");
		for(const auto &combination : columnCombinationsToCheck_){
			std::vector<int> displayIndices;
			displayIndices.reserve(combination.size());

			for(const ColumnOffset columnOffset : combination){
				displayIndices.push_back(columnOffset + 1);
			}

			fmt::println("[{}]", fmt::join(displayIndices, ":"));
		}
		return;
	}

	fmt::println("\n--- Define Field Combinations ---");
	fmt::println("Enter combinations of the field numbers above, separated by colons (e.g., 1:3 or 1:3:4).");

	std::vector<int> sortedColumnIdentifiers;
	sortedColumnIdentifiers.reserve(columns_.size());

	for(const auto &columnEntry : columns_){
		sortedColumnIdentifiers.push_back(columnEntry.first);
	}

	std::sort(sortedColumnIdentifiers.begin(), sortedColumnIdentifiers.end());

	if(!sortedColumnIdentifiers.empty()){
		fmt::println(
			"Default combination uses all selected fields: [{}]",
			fmt::join(sortedColumnIdentifiers, ":")
		);
	}

	fmt::println("Provide all combinations as a single comma separated list (e.g., 1, 3, 1:3), or press Enter to use the default above.");

	clearInputBuffer();
	std::string combinationInput;
	std::getline(std::cin, combinationInput);

	columnCombinationsToCheck_.clear();

	const bool hasNonWhitespaceInput{
		std::any_of(
			combinationInput.begin(),
			combinationInput.end(),
			[](unsigned char character){ return !std::isspace(character);}
		)
	};

	if(!hasNonWhitespaceInput){
		if(sortedColumnIdentifiers.empty()){
			throw std::runtime_error{"No fields were selected. Cannot define combinations."};
		}

		ColumnCombination defaultCombination;
		defaultCombination.reserve(sortedColumnIdentifiers.size());

		for(const int columnIdentifier : sortedColumnIdentifiers){
			const Column &columnDefinition{columns_.at(columnIdentifier)};
			defaultCombination.push_back(columnDefinition.index);
		}

		columnCombinationsToCheck_.push_back(std::move(defaultCombination));
		fmt::println(
			"Using default combination: [{}]",
			fmt::join(sortedColumnIdentifiers, ":")
		);
		return;
	}

	DelimitedStringList combinationStrings{splitString(combinationInput, ',')};

	for(const std::string &combinationString : combinationStrings){
		DelimitedStringList indexStrings{splitString(combinationString, ':')};

		if(indexStrings.empty()) continue;
		

		ColumnCombination currentCombination;
		currentCombination.reserve(indexStrings.size());
		bool isCombinationValid{true};

		for(const std::string &indexString : indexStrings){
			try{
				int fieldNumber{std::stoi(indexString)};
				if(!columns_.contains(fieldNumber)){
					fmt::println(stderr, "Field number {} in '{}' was not selected. Skipping this combination.", fieldNumber, combinationString);
					isCombinationValid = false;
					break;
				}

				currentCombination.push_back(fieldNumber - 1);
			}catch(const std::exception &){
				fmt::println(stderr, "Invalid number '{}' in combination. Skipping.\n", indexString);
				isCombinationValid = false;
				break;
			}
		}

		if(isCombinationValid && !currentCombination.empty()){
			std::sort(currentCombination.begin(), currentCombination.end());
			columnCombinationsToCheck_.push_back(std::move(currentCombination));
		}
	}

	if(columnCombinationsToCheck_.empty()){
		throw std::runtime_error{"No valid combinations were provided."};
	}
}