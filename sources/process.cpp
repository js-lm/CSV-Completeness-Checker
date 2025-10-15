#include "nanalyzer.hpp"

#include <csv.h>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include "constants.hpp"

void NaNalyzer::processCsvRows(
	std::chrono::steady_clock::duration updateInterval,
	std::atomic<long long int> &processedRowCount,
	long long int &totalRowCount,
	std::atomic<bool> &processingComplete,
	std::exception_ptr &workerException
){
	auto lastProgressUpdate{std::chrono::steady_clock::now()};
	long long int totalRowCountLocal{0};

	try{
		io::LineReader csvLineReader{csvFilePath_};
		try{
			char *headerLine{csvLineReader.next_line()};
			if(!headerLine){
				throw std::runtime_error{"No header line found in CSV file."};
			}
		}catch(const std::exception &exception){
			throw std::runtime_error{fmt::format(
				"Could not open or read file '{}'.\nDetails: {}",
				csvFilePath_,
				exception.what()
			)};
		}

		char *currentLine{nullptr};
		while((currentLine = csvLineReader.next_line()) != nullptr){
			totalRowCountLocal += 1;

			DelimitedStringList rowFields{splitString(std::string{currentLine}, ',')};

			for(std::size_t combinationIndex{0}; combinationIndex < columnCombinationsToCheck_.size(); combinationIndex++){
				const ColumnCombination &columnCombination{columnCombinationsToCheck_[combinationIndex]};
				bool isCombinationSatisfied{true};

				for(const ColumnDisjunction &clause : columnCombination){
					bool isClauseSatisfied{false};

					for(const ColumnOffset columnOffset : clause){
						const Column &columnDefinition{columns_.at(columnOffset + 1)};

						if(columnOffset >= static_cast<int>(rowFields.size())) continue;

						if(isCellValid(rowFields[columnOffset], columnDefinition.invalidValues)){
							isClauseSatisfied = true;
							break;
						}
					}

					if(!isClauseSatisfied){
						isCombinationSatisfied = false;
						break;
					}
				}

				if(isCombinationSatisfied) validCounts_[combinationIndex] += 1;
			}

			const auto now{std::chrono::steady_clock::now()};
			if(now - lastProgressUpdate >= updateInterval){
				processedRowCount.store(totalRowCountLocal, std::memory_order_relaxed);
				lastProgressUpdate = now;
			}
		}

		processedRowCount.store(totalRowCountLocal, std::memory_order_relaxed);
		totalRowCount = totalRowCountLocal;
	}catch(...){
		workerException = std::current_exception();
	}

	processingComplete.store(true, std::memory_order_release);
}

void NaNalyzer::process(){
	if(columnCombinationsToCheck_.empty()){
		throw std::runtime_error{"No column combinations were provided."};
	}

	static_assert(Constants::ProgressUpdateInterval.count() > 0, "Progress update interval must be positive.");
	const auto updateInterval{Constants::ProgressUpdateInterval};

	if(!silentMode_) fmt::println("\nProcessing...");

	validCounts_.assign(columnCombinationsToCheck_.size(), 0);

	std::atomic<long long int> processedRowCount{0};
	std::atomic<bool> processingComplete{false};
	std::exception_ptr workerException{nullptr};
	long long int totalRowCount{0};

	auto processingThread{std::thread{
		&NaNalyzer::processCsvRows,
		this,
		std::chrono::duration_cast<std::chrono::steady_clock::duration>(updateInterval),
		std::ref(processedRowCount),
		std::ref(totalRowCount),
		std::ref(processingComplete),
		std::ref(workerException)
	}};

	const auto computedPollInterval{std::min(
		std::chrono::duration_cast<std::chrono::milliseconds>(updateInterval),
		Constants::ProgressDefaultPollInterval
	)};
	const auto pollInterval{computedPollInterval.count() > 0 ? computedPollInterval : Constants::ProgressMinimumPollInterval};

	long long int lastDisplayedRowCount{0};
	std::size_t maxProgressMessageWidth{0};
	bool hasDisplayedProgress{false};
	auto nextProgressDisplay{std::chrono::steady_clock::now() + updateInterval};

	while(!processingComplete.load(std::memory_order_acquire)){
		const auto now{std::chrono::steady_clock::now()};
		if(now >= nextProgressDisplay){
			const long long int rowsProcessed{processedRowCount.load(std::memory_order_relaxed)};
			if(!silentMode_ && rowsProcessed > 0 && rowsProcessed != lastDisplayedRowCount){
				const std::string progressMessage{fmt::format("Processed {} rows...", rowsProcessed)};
				if(progressMessage.size() > maxProgressMessageWidth){
					maxProgressMessageWidth = progressMessage.size();
				}
				fmt::print("\r{:<{}}", progressMessage, maxProgressMessageWidth);
				std::fflush(stdout);
				lastDisplayedRowCount = rowsProcessed;
				hasDisplayedProgress = true;
			}

			nextProgressDisplay = now + updateInterval;
		}

		std::this_thread::sleep_for(pollInterval);
	}

	processingThread.join();

	if(workerException){
		std::rethrow_exception(workerException);
	}

	const long long int finalRowsProcessed{processedRowCount.load(std::memory_order_relaxed)};
	if(!silentMode_ && finalRowsProcessed > lastDisplayedRowCount && finalRowsProcessed > 0){
		const std::string progressMessage{fmt::format("Processed {} rows...", finalRowsProcessed)};
		if(progressMessage.size() > maxProgressMessageWidth){
			maxProgressMessageWidth = progressMessage.size();
		}
		fmt::print("\r{:<{}}", progressMessage, maxProgressMessageWidth);
		std::fflush(stdout);
		hasDisplayedProgress = true;
	}
	if(!silentMode_ && hasDisplayedProgress) fmt::print("\n");

	if(!silentMode_) fmt::println("\n--- Results ---");

	if(totalRowCount == 0){
		if(!silentMode_){
			fmt::println("No data rows found to process.");
			fmt::println("\nDone.");
		}
		return;
	}

	if(!silentMode_){
		fmt::println("Processed {} data rows.\n", totalRowCount);
	}

	for(std::size_t combinationIndex{0}; combinationIndex < columnCombinationsToCheck_.size(); combinationIndex++){
		const long long int validRowCount{validCounts_[combinationIndex]};
		double completenessPercentage{.0};
		if(totalRowCount > 0){
			completenessPercentage = (static_cast<double>(validRowCount) / static_cast<double>(totalRowCount)) * 100.0;
		}

		fmt::println(
			"[{}] : {} / {} ({:.2f}%)",
			formatCombinationForDisplay(columnCombinationsToCheck_[combinationIndex]),
			validRowCount,
			totalRowCount,
			completenessPercentage
		);
	}

	if(!silentMode_) fmt::println("\nDone.");
}