#include "nanalyzer.hpp"

#include <iostream>

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
			fmt::println("[{}]", formatCombinationForDisplay(combination));
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
	fmt::println("Enter the field numbers to include, separated by commas (e.g., 1, 2, 6).");
	fmt::println("Leave empty to select all fields.");

	const auto trimWhitespace{[](const std::string &value) -> std::string{
		const std::size_t firstNonWhitespace{value.find_first_not_of(" \t\n\r")};
		if(firstNonWhitespace == std::string::npos) return {};
		const std::size_t lastNonWhitespace{value.find_last_not_of(" \t\n\r")};
		return value.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
	}};

	while(columns_.empty()){
		fmt::print("Field numbers: ");
		if(!std::cin.good()) std::cin.clear();
		if(std::cin.peek() == '\n') std::cin.ignore();
		
		std::string selectionInput;
		if(!std::getline(std::cin, selectionInput)){
			throw std::runtime_error{"Failed to read field selection input."};
		}

		const bool hasNonWhitespaceInput{
			std::any_of(
				selectionInput.begin(),
				selectionInput.end(),
				[](unsigned char character){ return !std::isspace(character);} 
			)
		};

		if(!hasNonWhitespaceInput){
			for(std::size_t i{0}; i < headers_.size(); i++){
				Column columnDefinition;
				columnDefinition.index = i;
				columnDefinition.name = headers_[i];
				columns_.emplace(static_cast<int>(i + 1), std::move(columnDefinition));
			}
			fmt::println("Selected all {} fields.", headers_.size());
			continue;
		}

		DelimitedStringList selectedTokens{splitString(selectionInput, ',')};
		std::unordered_set<int> selectedFieldNumbers;
		bool isInputValid{true};

		for(const std::string &token : selectedTokens){
			const std::string trimmedToken{trimWhitespace(token)};
			if(trimmedToken.empty()){
				fmt::println(stderr, "Found an empty entry between commas. Please provide valid field numbers.");
				isInputValid = false;
				break;
			}

			try{
				int fieldNumber{std::stoi(trimmedToken)};
				if(fieldNumber < 1 || fieldNumber > static_cast<int>(headers_.size())){
					fmt::println(stderr, "Field {} is out of range. Valid range is 1 to {}.", fieldNumber, headers_.size());
					isInputValid = false;
					break;
				}

				selectedFieldNumbers.insert(fieldNumber);
			}catch(const std::exception &){
				fmt::println(stderr, "'{}' is not a valid number. Please try again.", trimmedToken);
				isInputValid = false;
				break;
			}
		}

		if(!isInputValid){
			columns_.clear();
			continue;
		}

		if(selectedFieldNumbers.empty()){
			fmt::println(stderr, "No valid field numbers were detected. Please try again.");
			continue;
		}

		for(const int fieldNumber : selectedFieldNumbers){
			Column columnDefinition;
			columnDefinition.index = fieldNumber - 1;
			columnDefinition.name = headers_[fieldNumber - 1];
			columns_.emplace(fieldNumber, std::move(columnDefinition));
		}
	}

	std::vector<int> sortedColumnIdentifiers;
	sortedColumnIdentifiers.reserve(columns_.size());

	for(const auto &columnEntry : columns_){
		sortedColumnIdentifiers.push_back(columnEntry.first);
	}

	std::sort(sortedColumnIdentifiers.begin(), sortedColumnIdentifiers.end());

	fmt::println("\n--- Selected Fields ---");
	for(const int columnIdentifier : sortedColumnIdentifiers){
		const Column &columnDefinition{columns_.at(columnIdentifier)};
		fmt::println("[{}] {}", columnIdentifier, columnDefinition.name);
	}

	fmt::println("\n--- Define Invalid or Empty Values ---");
	fmt::println("Select a field number to add invalid values, or type 'done' when finished.");

	while(true){
		fmt::println("\nCurrent invalid value definitions:");
		for(const int columnIdentifier : sortedColumnIdentifiers){
			const Column &columnDefinition{columns_.at(columnIdentifier)};
			std::vector<std::string> invalidValuesDisplay{columnDefinition.invalidValues.begin(), columnDefinition.invalidValues.end()};
			std::sort(invalidValuesDisplay.begin(), invalidValuesDisplay.end());
			std::string invalidValuesJoined;
			if(!invalidValuesDisplay.empty()){
				invalidValuesJoined = fmt::format("{}", fmt::join(invalidValuesDisplay, ", "));
			}

			fmt::println(
				"[{}] {}{}",
				columnIdentifier,
				columnDefinition.name,
				invalidValuesJoined.empty() ? ":" : fmt::format(": {}", invalidValuesJoined)
			);
		}

		fmt::print("Field number to update (or type 'done'): ");
		std::string selection;
		std::getline(std::cin, selection);
		std::string trimmedSelection{trimWhitespace(selection)};

		std::string loweredSelection{trimmedSelection};
		std::transform(
			loweredSelection.begin(), loweredSelection.end(), loweredSelection.begin(),
			[](unsigned char character){ return static_cast<char>(std::tolower(character));}
		);

		if(loweredSelection == "done") break;

		if(trimmedSelection.empty()){
			fmt::println(stderr, "Please enter a field number or type 'done' to finish.");
			continue;
		}

		int selectedFieldNumber{0};
		try{
			selectedFieldNumber = std::stoi(trimmedSelection);
		}catch(const std::exception &){
			fmt::println(stderr, "'{}' is not a valid number. Please choose one of the listed fields.", trimmedSelection);
			continue;
		}

		if(!columns_.contains(selectedFieldNumber)){
			fmt::println(stderr, "Field {} is not in the selected list. Please choose one of the listed fields.", selectedFieldNumber);
			continue;
		}

		Column &selectedColumn{columns_.at(selectedFieldNumber)};
		bool replaceExisting{false};

		if(selectedColumn.invalidValues.empty()){
			fmt::println("'{}' doesn't have any invalid values yet. New entries will be added to the list.", selectedColumn.name);
		}else{
			while(true){
				fmt::print(
					"Would you like to replace the existing invalid values for '{}' or add to them? (replace/add) [add]: ",
					selectedColumn.name
				);
				std::string actionInput;
				std::getline(std::cin, actionInput);
				std::string trimmedAction{trimWhitespace(actionInput)};
				std::transform(
					trimmedAction.begin(), trimmedAction.end(), trimmedAction.begin(),
					[](unsigned char character){ return static_cast<char>(std::tolower(character));}
				);

				if(trimmedAction.empty() || trimmedAction.starts_with('a')){
					replaceExisting = false;
					break;
				}

				if(trimmedAction.starts_with('r')){
					replaceExisting = true;
					break;
				}

				fmt::println(stderr, "Unrecognized choice '{}'. Please type 'replace', 'add', or press Enter for the default (add).", actionInput);
			}
		}

		fmt::print(
			"Enter invalid values for '{}' (comma separated, {}): ",
			selectedColumn.name,
			replaceExisting ? "leave blank to clear the list" : "leave blank to keep current list"
		);
		std::string invalidValuesInput;
		std::getline(std::cin, invalidValuesInput);

		const bool hasMeaningfulInput{
			std::any_of(
				invalidValuesInput.begin(),
				invalidValuesInput.end(),
				[](unsigned char character){ return !std::isspace(character);} 
			)
		};

		if(!hasMeaningfulInput){
			if(replaceExisting){
				selectedColumn.invalidValues.clear();
				fmt::println("Cleared invalid values for '{}'.", selectedColumn.name);
			}else{
				fmt::println("No new values provided for '{}'. Keeping existing list.", selectedColumn.name);
			}
			continue;
		}

		DelimitedStringList invalidValuesList{splitString(invalidValuesInput, ',')};
		bool addedValues{false};

		if(replaceExisting){
			selectedColumn.invalidValues.clear();
		}

		for(const std::string &value : invalidValuesList){
			const std::string trimmedValue{trimWhitespace(value)};
			if(trimmedValue.empty()) continue;

			if(selectedColumn.invalidValues.insert(trimmedValue).second){
				addedValues = true;
			}
		}

		if(addedValues){
			const std::string message{
				replaceExisting ? "Set invalid values for '{}' to the provided list."
					: "Updated invalid values for '{}'."
			};
			fmt::println(fmt::runtime(message), selectedColumn.name);
		}else{
			if(replaceExisting){
				fmt::println("No non-empty entries were provided. '{}' now has an empty invalid value list.", selectedColumn.name);
			}else{
				fmt::println("No new distinct values were added for '{}'.", selectedColumn.name);
			}
		}
	}

	fmt::println("\n--- Final invalid value definitions ---");
	for(const int columnIdentifier : sortedColumnIdentifiers){
		const Column &columnDefinition{columns_.at(columnIdentifier)};
		std::vector<std::string> invalidValuesDisplay{columnDefinition.invalidValues.begin(), columnDefinition.invalidValues.end()};
		std::sort(invalidValuesDisplay.begin(), invalidValuesDisplay.end());
		std::string invalidValuesJoined;
		if(!invalidValuesDisplay.empty()){
			invalidValuesJoined = fmt::format("{}", fmt::join(invalidValuesDisplay, ", "));
		}
		fmt::println(
			"[{}] {}{}",
			columnIdentifier,
			columnDefinition.name,
			invalidValuesJoined.empty() ? ":" : fmt::format(": {}", invalidValuesJoined)
		);
	}
}

void NaNalyzer::defineColumnCombinations(){
	if(configurationLoadedFromJson_ && !columnCombinationsToCheck_.empty()){
		fmt::println("\n--- Field combinations loaded from configuration ---");
		for(const auto &combination : columnCombinationsToCheck_){
			fmt::println("[{}]", formatCombinationForDisplay(combination));
		}
		return;
	}

	fmt::println("\n--- Define Field Combinations ---");

	std::vector<int> sortedColumnIdentifiers;
	sortedColumnIdentifiers.reserve(columns_.size());

	for(const auto &columnEntry : columns_){
		sortedColumnIdentifiers.push_back(columnEntry.first);
	}

	std::sort(sortedColumnIdentifiers.begin(), sortedColumnIdentifiers.end());

	std::string defaultCombinationDisplay;
	if(!sortedColumnIdentifiers.empty()){
		defaultCombinationDisplay = fmt::format("{}", fmt::join(sortedColumnIdentifiers, ":"));
	}

	fmt::println("Enter field combinations (\":\" = together, \"/\" = alternatives, \",\" = separate rules)");
	fmt::println("Example: 1:2:3/4 means fields 1 & 2 and either 3 or 4.");
	fmt::println("Multiple combos: 1, 2:3, 1:2:3/4");
	if(!defaultCombinationDisplay.empty()){
		fmt::println("Leave empty to use all fields [{}].", defaultCombinationDisplay);
	}

	fmt::print("Combinations: ");

	std::string combinationInput;
	if(!std::getline(std::cin, combinationInput)){
		throw std::runtime_error{"Failed to read field combination input."};
	}

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
			ColumnDisjunction clause;
			clause.push_back(columnDefinition.index);
			defaultCombination.push_back(std::move(clause));
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
		DelimitedStringList clauseStrings{splitString(combinationString, ':')};

		if(clauseStrings.empty()) continue;

		ColumnCombination currentCombination;
		currentCombination.reserve(clauseStrings.size());
		bool isCombinationValid{true};

		for(const std::string &clauseString : clauseStrings){
			if(clauseString.empty()){
				fmt::println(stderr, "Empty field group found in '{}'. Skipping this combination.", combinationString);
				isCombinationValid = false;
				break;
			}

			DelimitedStringList candidateStrings{splitString(clauseString, '/')};
			if(candidateStrings.empty()){
				isCombinationValid = false;
				break;
			}

			ColumnDisjunction disjunction;
			disjunction.reserve(candidateStrings.size());

			for(const std::string &candidate : candidateStrings){
				if(candidate.empty()){
					fmt::println(stderr, "Empty field entry in group '{}' within '{}'.", clauseString, combinationString);
					isCombinationValid = false;
					break;
				}

				try{
					int fieldNumber{std::stoi(candidate)};
					if(!columns_.contains(fieldNumber)){
						fmt::println(stderr, "Field number {} in '{}' was not selected. Skipping this combination.", fieldNumber, combinationString);
						isCombinationValid = false;
						break;
					}

					const int zeroBasedIndex{fieldNumber - 1};
					disjunction.push_back(zeroBasedIndex);
				}catch(const std::exception &){
					fmt::println(stderr, "Invalid number '{}' in combination '{}'. Skipping.", candidate, combinationString);
					isCombinationValid = false;
					break;
				}
			}

			if(!isCombinationValid) break;

			if(disjunction.empty()){
				isCombinationValid = false;
				break;
			}

			std::sort(disjunction.begin(), disjunction.end());
			disjunction.erase(std::unique(disjunction.begin(), disjunction.end()), disjunction.end());
			currentCombination.push_back(std::move(disjunction));
		}

		if(isCombinationValid && !currentCombination.empty()){
			columnCombinationsToCheck_.push_back(std::move(currentCombination));
		}
	}

	if(columnCombinationsToCheck_.empty()){
		throw std::runtime_error{"No valid combinations were provided."};
	}
}