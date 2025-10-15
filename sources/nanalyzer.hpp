#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <chrono>
#include <exception>
#include <optional>

struct CLIConfig{
    std::optional<std::string> csvFilePath;
    std::optional<std::string> configFilePath;
    std::optional<std::string> outputFilePath;
    std::optional<std::string> fieldsInput;
    std::optional<std::string> invalidValuesInput;
    std::optional<std::string> combinationsInput;
    bool silent{false};
};

class NaNalyzer{
    using ColumnOffset = int;   // 0 based position in headers_ and CSV rows
    using ColumnNumber = int;   // 1 based identifier presented to users

    using FilePath = std::string;
    
    using HeaderList = std::vector<std::string>;
    using InvalidValueSet = std::unordered_set<std::string>;
    
    using DelimitedStringList = std::vector<std::string>;

    using ColumnDisjunction = std::vector<ColumnOffset>; // OR group
    using ColumnCombination = std::vector<ColumnDisjunction>; // AND of OR groups
    using CombinationList = std::vector<ColumnCombination>;

    using ValidCounts = std::vector<long long int>;

private:
    FilePath csvFilePath_;

    HeaderList headers_;
    struct Column{
        ColumnOffset index; // 0 based corresponding to headers_
        std::string name;
        InvalidValueSet invalidValues;
    };
    using ColumnMap = std::unordered_map<ColumnNumber, Column>;
    ColumnMap columns_;

    CombinationList columnCombinationsToCheck_;
    ValidCounts validCounts_;

private:
    bool configurationLoadedFromJson_{false};
    bool silentMode_{false};

public:
    NaNalyzer() = default;
    ~NaNalyzer() = default;

    int run(const CLIConfig &config = CLIConfig{});

    void saveInitializationToJson(const FilePath &filePath) const;
    void loadInitializationFromJson(const FilePath &filePath);

private:
    void parseCsv();
    void defineInvalidData();
    void defineColumnCombinations();

    void process();
    void processCsvRows(
        std::chrono::steady_clock::duration updateInterval,
        std::atomic<long long int> &processedRowCount,
        long long int &totalRowCount,
        std::atomic<bool> &processingComplete,
        std::exception_ptr &workerException
    );

private:
    DelimitedStringList splitString(const std::string &string, const char delimiter) const;

    void clearInputBuffer() const;

    bool isCellValid(const std::string &string, const InvalidValueSet &invalidValues) const;

    std::string formatCombinationForDisplay(const ColumnCombination &combination) const;
};