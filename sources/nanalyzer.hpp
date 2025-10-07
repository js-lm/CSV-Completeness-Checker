#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>

class NaNalyzer{
    using ColumnIndex = int;

private:
    std::string csvFilePath_;

    std::vector<std::string> headers_;
    struct Column{
        ColumnIndex index; // 0 based corresponding to headers_
        std::string name;
        std::unordered_set<std::string> invalidValues;
    };
    std::unordered_map<ColumnIndex, Column> columns_;

    std::vector<std::vector<ColumnIndex>> columnCombinationsToCheck_;
    std::vector<long long int> validCounts_;

public:
    NaNalyzer() = default;
    ~NaNalyzer() = default;

    void run();

private:
    void parseCsv();
    void defineInvalidData();
    void defineColumnCombinations();

    void process();

private:
    bool isCellValid(const std::string &string, const std::unordered_set<std::string> &invalidValues) const;

};