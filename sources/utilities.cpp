#include "nanalyzer.hpp"

#include <iostream>
#include <limits>
#include <sstream>

#include <fmt/core.h>
#include <fmt/ranges.h>

NaNalyzer::DelimitedStringList NaNalyzer::splitString(
    const std::string &string, const char delimiter
) const{
    DelimitedStringList tokens;
    std::string currentToken;
    std::istringstream tokenStream{string};

    while(std::getline(tokenStream, currentToken, delimiter)){
        std::size_t firstNonWhitespacePosition{currentToken.find_first_not_of(" \t\n\r")};
        if(firstNonWhitespacePosition == std::string::npos){
            tokens.emplace_back();
            continue;
        }

        std::size_t lastNonWhitespacePosition{currentToken.find_last_not_of(" \t\n\r")};
        std::string trimmedToken{currentToken.substr(firstNonWhitespacePosition, lastNonWhitespacePosition - firstNonWhitespacePosition + 1)};

        tokens.push_back(std::move(trimmedToken));
    }

    return tokens;
}

bool NaNalyzer::isCellValid(
    const std::string &string, 
    const InvalidValueSet &invalidValues
) const{
    return !string.empty() && invalidValues.find(string) == invalidValues.end();
}

void NaNalyzer::clearInputBuffer() const{
    if(!std::cin.good()) std::cin.clear();
    std::streambuf *inputBuffer{std::cin.rdbuf()};
    if(inputBuffer->in_avail() == 0) return;

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

std::string NaNalyzer::formatCombinationForDisplay(
    const ColumnCombination &combination
) const{
    std::vector<std::string> clauseStrings;
    clauseStrings.reserve(combination.size());

    for(const auto &disjunction : combination){
        std::vector<int> displayIndices;
        displayIndices.reserve(disjunction.size());

        for(const ColumnOffset columnOffset : disjunction){
            displayIndices.push_back(columnOffset + 1);
        }

        clauseStrings.push_back(fmt::format("{}", fmt::join(displayIndices, "/")));
    }

    return fmt::format("{}", fmt::join(clauseStrings, ":"));
}