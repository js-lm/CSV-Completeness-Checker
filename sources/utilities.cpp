#include "nanalyzer.hpp"

#include <sstream>
#include <iostream>
#include <limits>

std::vector<std::string> NaNalyzer::splitString(
    const std::string &string, const char delimiter
) const{
    std::vector<std::string> tokens;
    std::string currentToken;
    std::istringstream tokenStream{string};

    while(std::getline(tokenStream, currentToken, delimiter)){
        std::size_t firstNonWhitespacePosition{currentToken.find_first_not_of(" \t\n\r")};
        if(firstNonWhitespacePosition == std::string::npos){
            continue;
        }

        std::size_t lastNonWhitespacePosition{currentToken.find_last_not_of(" \t\n\r")};
        std::string trimmedToken{currentToken.substr(firstNonWhitespacePosition, lastNonWhitespacePosition - firstNonWhitespacePosition + 1)};

        if(!trimmedToken.empty()){
            tokens.push_back(trimmedToken);
        }
    }

    return tokens;
}

bool NaNalyzer::isCellValid(
    const std::string &string, 
    const std::unordered_set<std::string> &invalidValues
) const{
    return !string.empty() && invalidValues.find(string) == invalidValues.end();
}

void NaNalyzer::clearInputBuffer() const{
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}