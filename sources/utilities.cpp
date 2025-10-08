#include "nanalyzer.hpp"

#include <sstream>
#include <iostream>
#include <limits>

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
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}