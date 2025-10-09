#include "nanalyzer.hpp"

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <csv.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <vector>
#include <string>
#include <unordered_set>
#include <stdexcept>
#include <utility>
#include <algorithm>

#include "constants.hpp"

void NaNalyzer::saveInitializationToJson(const std::string &filePath) const{
    if(csvFilePath_.empty()){
        throw std::runtime_error{"No source CSV file specified. Nothing to export."};
    }

    nlohmann::json root;
    root["version"] = Constants::Version;
    root["csv_file"] = csvFilePath_;
    root["headers"] = headers_;

    std::vector<std::pair<int, Column>> sortedColumns{columns_.begin(), columns_.end()};
    std::sort(sortedColumns.begin(), sortedColumns.end(), [](const auto &a, const auto &b){
        return a.first < b.first;
    });

    nlohmann::json columnsJson{nlohmann::json::array()};
    for(const auto &[fieldNumber, columnDefinition] : sortedColumns){
        nlohmann::json columnJson;
        columnJson["field_number"] = fieldNumber;
        columnJson["name"] = columnDefinition.name;

        DelimitedStringList invalidValues{columnDefinition.invalidValues.begin(), columnDefinition.invalidValues.end()};
        std::sort(invalidValues.begin(), invalidValues.end());
        columnJson["invalid_values"] = std::move(invalidValues);

        columnsJson.push_back(std::move(columnJson));
    }

    root["columns"] = std::move(columnsJson);

    nlohmann::json combinationsJson{nlohmann::json::array()};
    for(const auto &combination : columnCombinationsToCheck_){
        const bool hasDisjunction{
            std::any_of(
                combination.begin(),
                combination.end(),
                [](const ColumnDisjunction &clause){ return clause.size() > 1;}
            )
        };

        if(!hasDisjunction){
            std::vector<int> flattened;
            flattened.reserve(combination.size());

            for(const ColumnDisjunction &clause : combination){
                if(clause.empty()) continue;
                flattened.push_back(clause.front() + 1);
            }

            combinationsJson.push_back(std::move(flattened));
            continue;
        }

        nlohmann::json combinationJson{nlohmann::json::array()};
        for(const ColumnDisjunction &clause : combination){
            std::vector<int> clauseIndices;
            clauseIndices.reserve(clause.size());

            for(const ColumnOffset columnOffset : clause){
                clauseIndices.push_back(columnOffset + 1);
            }

            combinationJson.push_back(std::move(clauseIndices));
        }

        combinationsJson.push_back(std::move(combinationJson));
    }

    root["combinations"] = std::move(combinationsJson);

    std::ofstream outputFile{filePath};
    if(!outputFile){
        throw std::runtime_error{fmt::format("Could not open '{}' for writing.", filePath)};
    }

    outputFile << root.dump(4) << '\n';
}

void NaNalyzer::loadInitializationFromJson(const std::string &filePath){
    std::ifstream inputFile{filePath};
    if(!inputFile){
        throw std::runtime_error{fmt::format("Could not open JSON configuration file '{}'.", filePath)};
    }

    nlohmann::json root;
    try{
        inputFile >> root;
    }catch(const nlohmann::json::exception &exception){
        throw std::runtime_error{fmt::format("Failed to parse JSON configuration '{}'. {}", filePath, exception.what())};
    }

    const std::string csvPath{root.at("csv_file").get<std::string>()};
    if(csvPath.empty()){
        throw std::runtime_error{"JSON configuration is missing 'csv_file'."};
    }

    io::LineReader csvLineReader{csvPath};
    char *headerLine{nullptr};
    try{
        headerLine = csvLineReader.next_line();
    }catch(const std::exception &exception){
        throw std::runtime_error{fmt::format("Could not open or read file '{}'. {}", csvPath, exception.what())};
    }

    if(!headerLine){
        throw std::runtime_error{"No header line found in CSV file referenced by configuration."};
    }

    HeaderList actualHeaders{splitString(std::string{headerLine}, ',')};
    if(actualHeaders.empty()){
        throw std::runtime_error{"CSV file referenced by configuration does not contain any headers."};
    }

    if(root.contains("headers")){
        const HeaderList headersFromJson{root["headers"].get<HeaderList>()};
        if(!headersFromJson.empty() && headersFromJson != actualHeaders){
            throw std::runtime_error{"Headers in JSON configuration do not match the CSV file."};
        }
    }

    csvFilePath_ = csvPath;
    headers_ = std::move(actualHeaders);

    columns_.clear();
    if(root.contains("columns")){
        for(const auto &columnJson : root["columns"]){
            if(!columnJson.contains("field_number")) throw std::runtime_error{"Each column entry must include 'field_number'."};
        
            int fieldNumber{columnJson.at("field_number").get<int>()};
            if(fieldNumber < 1 || fieldNumber > static_cast<int>(headers_.size())){
                throw std::runtime_error{fmt::format("Column field_number {} is out of range for the CSV headers.", fieldNumber)};
            }

            Column columnDefinition;
            columnDefinition.index = fieldNumber - 1;
            columnDefinition.name = columnJson.value("name", headers_[columnDefinition.index]);

            DelimitedStringList invalidValues;
            if(columnJson.contains("invalid_values")){
                invalidValues = columnJson["invalid_values"].get<DelimitedStringList>();
            }
            columnDefinition.invalidValues = InvalidValueSet{invalidValues.begin(), invalidValues.end()};

            columns_.emplace(fieldNumber, std::move(columnDefinition));
        }
    }

    columnCombinationsToCheck_.clear();
    if(root.contains("combinations")){
        for(const auto &combinationJson : root["combinations"]){
            if(!combinationJson.is_array()) continue;

            ColumnCombination combination;

            const bool isLegacyFormat{
                std::all_of(
                    combinationJson.begin(),
                    combinationJson.end(),
                    [](const nlohmann::json &entry){ return entry.is_number_integer();}
                )
            };

            if(isLegacyFormat){
                combination.reserve(combinationJson.size());

                for(const auto &value : combinationJson){
                    int fieldNumber{value.get<int>()};
                    const int zeroBasedIndex{fieldNumber - 1};

                    if(zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int>(headers_.size())){
                        throw std::runtime_error{fmt::format("Field number {} in combinations is out of range.", fieldNumber)};
                    }

                    if(!columns_.contains(fieldNumber)){
                        throw std::runtime_error{fmt::format("Combination references field {} which is not selected.", fieldNumber)};
                    }

                    ColumnDisjunction clause;
                    clause.push_back(zeroBasedIndex);
                    combination.push_back(std::move(clause));
                }
            }else{
                combination.reserve(combinationJson.size());

                for(const auto &clauseJson : combinationJson){
                    ColumnDisjunction clause;

                    if(clauseJson.is_number_integer()){
                        int fieldNumber{clauseJson.get<int>()};
                        const int zeroBasedIndex{fieldNumber - 1};

                        if(zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int>(headers_.size())){
                            throw std::runtime_error{fmt::format("Field number {} in combinations is out of range.", fieldNumber)};
                        }

                        if(!columns_.contains(fieldNumber)){
                            throw std::runtime_error{fmt::format("Combination references field {} which is not selected.", fieldNumber)};
                        }

                        clause.push_back(zeroBasedIndex);
                    }else if(clauseJson.is_array()){
                        for(const auto &value : clauseJson){
                            if(!value.is_number_integer()){
                                throw std::runtime_error{"Combination entries must be integers."};
                            }

                            int fieldNumber{value.get<int>()};
                            const int zeroBasedIndex{fieldNumber - 1};

                            if(zeroBasedIndex < 0 || zeroBasedIndex >= static_cast<int>(headers_.size())){
                                throw std::runtime_error{fmt::format("Field number {} in combinations is out of range.", fieldNumber)};
                            }

                            if(!columns_.contains(fieldNumber)){
                                throw std::runtime_error{fmt::format("Combination references field {} which is not selected.", fieldNumber)};
                            }

                            clause.push_back(zeroBasedIndex);
                        }
                    }else{
                        throw std::runtime_error{"Combination groups must be integers or arrays of integers."};
                    }

                    if(clause.empty()){
                        throw std::runtime_error{"Encountered an empty combination group in JSON configuration."};
                    }

                    std::sort(clause.begin(), clause.end());
                    clause.erase(std::unique(clause.begin(), clause.end()), clause.end());
                    combination.push_back(std::move(clause));
                }
            }

            if(!combination.empty()){
                columnCombinationsToCheck_.push_back(std::move(combination));
            }
        }
    }

    configurationLoadedFromJson_ = true;
}
