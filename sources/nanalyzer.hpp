#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>

class NaNalyzer{
    using ColumnOffset = int;   // 0 based position in headers_ and CSV rows
    using ColumnNumber = int;   // 1 based identifier presented to users

    using FilePath = std::string;
    
    using HeaderList = std::vector<std::string>;
    using InvalidValueSet = std::unordered_set<std::string>;

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

public:
    NaNalyzer() = default;
    ~NaNalyzer() = default;

    int run();

    void saveInitializationToJson(const FilePath &filePath) const;
    void loadInitializationFromJson(const FilePath &filePath);

private:
    void parseCsv();
    void defineInvalidData();
    void defineColumnCombinations();

    void process();

private:
    using DelimitedStringList = std::vector<std::string>;
    DelimitedStringList splitString(const std::string &string, const char delimiter) const;

    void clearInputBuffer() const;

    bool isCellValid(const std::string &string, const InvalidValueSet &invalidValues) const;

    std::string formatCombinationForDisplay(const ColumnCombination &combination) const;

};