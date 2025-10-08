#include "nanalyzer.hpp"

#include <csv.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

void NaNalyzer::process(){
	if(columnCombinationsToCheck_.empty()){
		throw std::runtime_error{"No column combinations were provided."};
	}

	io::LineReader csvLineReader{csvFilePath_};
	try{
		char *headerLine{csvLineReader.next_line()};
		if(!headerLine){
			throw std::runtime_error{"No header line found in CSV file."};
		}
	}catch(const std::exception &exception){
		throw std::runtime_error{fmt::format("Could not open or read file '{}'.\nDetails: {}", csvFilePath_, exception.what())};
	}

	fmt::println("\nProcessing...");

	validCounts_.assign(columnCombinationsToCheck_.size(), 0);
	long long int totalRowCount{0};

	char *currentLine{nullptr};
	while((currentLine = csvLineReader.next_line()) != nullptr){
		totalRowCount += 1;
		
		DelimitedStringList rowFields{splitString(std::string{currentLine}, ',')};

		for(std::size_t combinationIndex{0}; combinationIndex < columnCombinationsToCheck_.size(); combinationIndex++){
			const ColumnCombination &columnCombination{columnCombinationsToCheck_[combinationIndex]};
			bool areAllFieldsValid{true};

			for(const ColumnOffset columnOffset : columnCombination){
				const Column &columnDefinition{columns_.at(columnOffset + 1)};
				
				if(columnOffset >= static_cast<int>(rowFields.size())){
					areAllFieldsValid = false;
					break;
				}
				
				if(!isCellValid(rowFields[columnOffset], columnDefinition.invalidValues)){
					areAllFieldsValid = false;
					break;
				}
			}

			if(areAllFieldsValid){
				validCounts_[combinationIndex] += 1;
			}
		}
	}

	fmt::println("\n--- Results ---");

	if(totalRowCount == 0){
		fmt::println("No data rows found to process.");
		fmt::println("\nDone.");
		return;
	}

	fmt::println("Processed {} data rows.\n", totalRowCount);

	for(std::size_t combinationIndex{0}; combinationIndex < columnCombinationsToCheck_.size(); combinationIndex++){
		std::vector<int> displayIndices;
		displayIndices.reserve(columnCombinationsToCheck_[combinationIndex].size());

		for(const ColumnOffset columnOffset : columnCombinationsToCheck_[combinationIndex]){
			displayIndices.push_back(columnOffset + 1);
		}

		const long long int validRowCount{validCounts_[combinationIndex]};
		double completenessPercentage{.0};
		if(totalRowCount > 0){
			completenessPercentage = (static_cast<double>(validRowCount) / static_cast<double>(totalRowCount)) * 100.0;
		}

		fmt::println(
			"[{}] : {} / {} ({:.2f}%)",
			fmt::join(displayIndices, ":"),
			validRowCount,
			totalRowCount,
			completenessPercentage
		);
	}

	fmt::println("\nDone.");
}