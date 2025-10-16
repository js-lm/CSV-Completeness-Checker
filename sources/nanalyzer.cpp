#include "nanalyzer.hpp"

#include <fmt/chrono.h>
#include <fmt/core.h>

#include <csv.h>
#include <iostream>
#include <nlohmann/json.hpp>

#include "constants.hpp"

int NaNalyzer::run(const CLIConfig &config){
	silentMode_ = config.silent;
	outputFormat_ = config.outputFormat;

	if(!silentMode_){
		fmt::println("{} v{}", Constants::Title, Constants::Version);
	}

	try{
		
		if(config.configFilePath.has_value()){ // --config
			try{
				loadInitializationFromJson(config.configFilePath.value());
				if(!silentMode_){
					fmt::println("Loaded initialization settings from '{}'.", config.configFilePath.value());
				}
			}catch(const std::exception &exception){
				throw std::runtime_error{fmt::format(
					"Failed to load initialization from '{}'. {}",
					config.configFilePath.value(),
					exception.what()
				)};
			}
		}else if(config.csvFilePath.has_value()){ // --csv			
			csvFilePath_ = config.csvFilePath.value();

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

			if(headers_.empty()){
				throw std::runtime_error{"No header line found in CSV file."};
			}

			if(!silentMode_){
				fmt::println("Source CSV file: {}", csvFilePath_);
			}
		}else{ // interactive
			parseCsv();
		}

		if(columns_.empty() && !config.fieldsInput.has_value()){
			if(config.csvFilePath.has_value()){
				for(std::size_t fieldNumber{0}; fieldNumber < headers_.size(); fieldNumber++){
					Column columnDefinition;
					columnDefinition.index = fieldNumber;
					columnDefinition.name = headers_[fieldNumber];
					columns_.emplace(static_cast<int>(fieldNumber + 1), std::move(columnDefinition));
				}
				if(!silentMode_){
					fmt::println("Selected all {} fields.", headers_.size());
				}
			}else{
				defineInvalidData();
			}
		}else if(config.fieldsInput.has_value()){
			if(!silentMode_){
				fmt::println("\n--- Processing fields from CLI ---");
			}
			
			DelimitedStringList fieldTokens{splitString(config.fieldsInput.value(), ',')};
			std::unordered_set<int> selectedFieldNumbers;

			for(const std::string &token : fieldTokens){
				const std::string trimmedToken{[&token](){
					const std::size_t firstNonWhitespace{token.find_first_not_of(" \t\n\r")};
					if(firstNonWhitespace == std::string::npos){
						return std::string{};
					}
					const std::size_t lastNonWhitespace{token.find_last_not_of(" \t\n\r")};
					return token.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
				}()};

				if(trimmedToken.empty()) continue;

				try{
					int fieldNumber{std::stoi(trimmedToken)};
					if(fieldNumber < 1 || fieldNumber > static_cast<int>(headers_.size())){
						throw std::runtime_error{fmt::format(
							"Field {} is out of range. Valid range is 1 to {}.",
							fieldNumber,
							headers_.size()
						)};
					}
					selectedFieldNumbers.insert(fieldNumber);
				}catch(const std::exception &exception){
					throw std::runtime_error{fmt::format("Invalid field number '{}': {}", trimmedToken, exception.what())};
				}
			}

			if(selectedFieldNumbers.empty()){
				throw std::runtime_error{"No valid field numbers were detected."};
			}

			columns_.clear();
			for(const int fieldNumber : selectedFieldNumbers){
				Column columnDefinition;
				columnDefinition.index = fieldNumber - 1;
				columnDefinition.name = headers_[fieldNumber - 1];
				columns_.emplace(fieldNumber, std::move(columnDefinition));
			}

			if(!silentMode_){
				fmt::println("Selected {} fields.", selectedFieldNumbers.size());
			}
		}

		if(config.invalidValuesInput.has_value()){
			if(!silentMode_){
				fmt::println("\n--- Processing invalid values from CLI ---");
			}

			const std::string &invalidValuesString{config.invalidValuesInput.value()};
			int currentField{-1};

			DelimitedStringList parts{splitString(invalidValuesString, ':')};
			for(const std::string &part : parts){
				const std::string trimmed{[&part](){
					const std::size_t firstNonWhitespace{part.find_first_not_of(" \t\n\r")};
					if(firstNonWhitespace == std::string::npos){
						return std::string{};
					}
					const std::size_t lastNonWhitespace{part.find_last_not_of(" \t\n\r")};
					return part.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
				}()};

				if(trimmed.empty()) continue;

				try{
					int fieldNumber{std::stoi(trimmed)};
					if(fieldNumber >= 1 && fieldNumber <= static_cast<int>(headers_.size()) && columns_.contains(fieldNumber)){
						currentField = fieldNumber;
						continue;
					}
				}catch(...){}

				if(currentField == -1){
					throw std::runtime_error{"Invalid values provided without specifying a field first."};
				}

				if(!columns_.contains(currentField)){
					throw std::runtime_error{fmt::format("Field {} not in selected columns.", currentField)};
				}

				columns_.at(currentField).invalidValues.insert(trimmed);
			}

			if(!silentMode_) fmt::println("Invalid values configured.");
		}

		if(columnCombinationsToCheck_.empty() && !config.combinationsInput.has_value()){
			if(config.csvFilePath.has_value()){
				ColumnCombination defaultCombination;
				for(const auto &columnEntry : columns_){
					ColumnDisjunction clause;
					clause.push_back(columnEntry.second.index);
					defaultCombination.push_back(std::move(clause));
				}
				if(!defaultCombination.empty()){
					columnCombinationsToCheck_.push_back(std::move(defaultCombination));
				}
				if(!silentMode_){
					fmt::println("Using all selected fields as default combination.");
				}
			}else{
				defineColumnCombinations();
			}
		}else if(config.combinationsInput.has_value()){
			if(!silentMode_){
				fmt::println("\n--- Processing combinations from CLI ---");
			}

			const std::string &combinationString{config.combinationsInput.value()};
			DelimitedStringList combinationGroups{splitString(combinationString, ',')};

			for(const std::string &groupString : combinationGroups){
				const std::string trimmedGroup{[&groupString](){
					const std::size_t firstNonWhitespace{groupString.find_first_not_of(" \t\n\r")};
					if(firstNonWhitespace == std::string::npos){
						return std::string{};
					}
					const std::size_t lastNonWhitespace{groupString.find_last_not_of(" \t\n\r")};
					return groupString.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
				}()};

				if(trimmedGroup.empty()) continue;

			ColumnCombination combination;
			DelimitedStringList andParts{splitString(trimmedGroup, ':')};

			for(const std::string &andPart : andParts){
				const std::string trimmedAndPart{[&andPart](){
						const std::size_t firstNonWhitespace{andPart.find_first_not_of(" \t\n\r")};
						if(firstNonWhitespace == std::string::npos){
							return std::string{};
						}
						const std::size_t lastNonWhitespace{andPart.find_last_not_of(" \t\n\r")};
						return andPart.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
					}()};

				ColumnDisjunction clause;
				DelimitedStringList orParts{splitString(trimmedAndPart, '/')};

				for(const std::string &orPart : orParts){
						const std::string trimmedOrPart{[&orPart](){
							const std::size_t firstNonWhitespace{orPart.find_first_not_of(" \t\n\r")};
							if(firstNonWhitespace == std::string::npos){
								return std::string{};
							}
							const std::size_t lastNonWhitespace{orPart.find_last_not_of(" \t\n\r")};
							return orPart.substr(firstNonWhitespace, lastNonWhitespace - firstNonWhitespace + 1);
						}()};

						if(trimmedOrPart.empty()) continue;

						try{
							int fieldNumber{std::stoi(trimmedOrPart)};
							int zeroBasedIndex{fieldNumber - 1};

							if(zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int>(headers_.size())){
								throw std::runtime_error{fmt::format("Field {} is out of range.", fieldNumber)};
							}

							if(!columns_.contains(fieldNumber)){
								throw std::runtime_error{fmt::format("Field {} not in selected columns.", fieldNumber)};
							}

							clause.push_back(zeroBasedIndex);
						}catch(const std::exception &exception){
							throw std::runtime_error{fmt::format("Invalid field in combination '{}': {}", trimmedOrPart, exception.what())};
						}
					}

					if(!clause.empty()){
						std::sort(clause.begin(), clause.end());
						clause.erase(std::unique(clause.begin(), clause.end()), clause.end());
						combination.push_back(std::move(clause));
					}
				}

				if(!combination.empty()){
					columnCombinationsToCheck_.push_back(std::move(combination));
				}
			}

			if(columnCombinationsToCheck_.empty()){
				throw std::runtime_error{"No valid combinations were parsed."};
			}

			if(!silentMode_){
				fmt::println("Configured {} combination(s).", columnCombinationsToCheck_.size());
			}
		}

		bool shouldProcess{true};

		if(!config.configFilePath.has_value() && !config.csvFilePath.has_value()){
			if(configurationLoadedFromJson_){
				fmt::print("\nProceed to processing the CSV now? (Y/n): ");
				std::string response;
				std::getline(std::cin, response);
				shouldProcess = [&response](){
					if(response.empty()) return true;
					const char firstCharacter{static_cast<char>(std::tolower(static_cast<unsigned char>(response.front())))};
					if(firstCharacter == 'y') return true;
					if(firstCharacter == 'n') return false;
					return true;
				}();
			}else{
				fmt::print("\nWould you like to save these initialization settings to a JSON file? (y/N): ");
				std::string saveResponse;
				std::getline(std::cin, saveResponse);

				auto parseYesNo{[](const std::string &input, const bool defaultValue){
					if(input.empty()) return defaultValue;
					const char firstCharacter{static_cast<char>(std::tolower(static_cast<unsigned char>(input.front())))};
					if(firstCharacter == 'y') return true;
					if(firstCharacter == 'n') return false;
					return defaultValue;
				}};

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
		}else if(config.outputFilePath.has_value()){
			try{
				saveInitializationToJson(config.outputFilePath.value());
				if(!silentMode_){
					fmt::println("Saved initialization settings to '{}'.", config.outputFilePath.value());
				}
			}catch(const std::exception &exception){
				fmt::println(stderr, "Failed to save initialization settings: {}", exception.what());
			}
		}

		if(shouldProcess){
			process();
		}else if(!silentMode_){
			fmt::println("Skipping processing per user request.");
		}

	}catch(const std::exception &error){
		fmt::println(stderr, "Fatal error: {}", error.what());
		return 1;
	}

	return 0;
}

std::string NaNalyzer::formatResultsAsJson(long long int totalRowCount) const{
	nlohmann::json root;
	root["version"] = Constants::Version;
	root["total_rows"] = totalRowCount;
	
	nlohmann::json resultsArray{nlohmann::json::array()};
	for(std::size_t combinationIndex{0}; combinationIndex < columnCombinationsToCheck_.size(); combinationIndex++){
		const long long int validRowCount{validCounts_[combinationIndex]};
		float completeness{.0f};
		if(totalRowCount > 0){
			completeness = static_cast<float>(validRowCount) / static_cast<float>(totalRowCount);
		}

		nlohmann::json resultObject;
		resultObject["combination"] = formatCombinationForDisplay(columnCombinationsToCheck_[combinationIndex]);
		resultObject["valid_rows"] = validRowCount;
		resultObject["total_rows"] = totalRowCount;
		resultObject["completeness"] = completeness;

		resultsArray.push_back(std::move(resultObject));
	}
	root["results"] = std::move(resultsArray);

	return root.dump(2);
}

std::string NaNalyzer::formatResultsAsCsv(long long int totalRowCount) const{
	std::string csvOutput;
	for(std::size_t combinationIndex{0}; combinationIndex < columnCombinationsToCheck_.size(); combinationIndex++){
		const long long int validRowCount{validCounts_[combinationIndex]};
		float completeness{.0f};
		if(totalRowCount > 0){
			completeness = static_cast<float>(validRowCount) / static_cast<float>(totalRowCount);
		}

		if(combinationIndex > 0) csvOutput += ',';

		csvOutput += fmt::format("{:.2f}", completeness);
	}
	return csvOutput;
}

std::string NaNalyzer::formatResultsAsKeyValue(long long int totalRowCount) const{
	std::string keyValueOutput;
	for(std::size_t combinationIndex{0}; combinationIndex < columnCombinationsToCheck_.size(); combinationIndex++){
		const long long int validRowCount{validCounts_[combinationIndex]};
		float completeness{.0f};
		if(totalRowCount > 0){
			completeness = static_cast<float>(validRowCount) / static_cast<float>(totalRowCount);
		}

		if(combinationIndex > 0) keyValueOutput += ' ';
		
		keyValueOutput += fmt::format("{}={:.2f}", 
			formatCombinationForDisplay(columnCombinationsToCheck_[combinationIndex]),
			completeness
		);
	}
	return keyValueOutput;
}
