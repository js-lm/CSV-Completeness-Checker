#include "nanalyzer.hpp"

#include <cxxopts.hpp>
#include <fmt/core.h>
#include <iostream>

int main(int argumentCount, char *arguments[]){
    cxxopts::Options options{
        "csv-completeness-checker",
        "Analyze CSV files for data completeness by checking for missing/invalid values"
    };

    options.add_options()
        ("c,csv", "Path to CSV file to analyze", cxxopts::value<std::string>())
        ("C,config", "Path to JSON configuration file (overrides --csv)", cxxopts::value<std::string>())
        ("o,output", "Path to save output JSON results", cxxopts::value<std::string>())
        ("f,fields", "Comma-separated field numbers to analyze (e.g., 1,2,3,5)", cxxopts::value<std::string>())
        ("i,invalid-values", "Invalid values mapping (format: field:value1,value2:field:value3...)", cxxopts::value<std::string>())
        ("b,combinations", "Column combinations to check (format: 1:2,1:3/4)", cxxopts::value<std::string>())
        ("format", "Output format: text, json, csv, or keyvalue (force quiet mode) (default: text)", cxxopts::value<std::string>()->default_value("text"))
        ("q,silent", "Minimal output (only results)", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print help")
    ;

    try{
        auto parseResult{options.parse(argumentCount, arguments)};

        if(parseResult.count("help")){
            std::cout << options.help() << std::endl;
            return 0;
        }

        CLIConfig config{};
        
        if(parseResult.count("csv")){
            config.csvFilePath = parseResult["csv"].as<std::string>();
        }
        if(parseResult.count("config")){
            config.configFilePath = parseResult["config"].as<std::string>();
        }
        if(parseResult.count("output")){
            config.outputFilePath = parseResult["output"].as<std::string>();
        }
        if(parseResult.count("fields")){
            config.fieldsInput = parseResult["fields"].as<std::string>();
        }
        if(parseResult.count("invalid-values")){
            config.invalidValuesInput = parseResult["invalid-values"].as<std::string>();
        }
        if(parseResult.count("combinations")){
            config.combinationsInput = parseResult["combinations"].as<std::string>();
        }
        if(parseResult.count("format")){
            std::string formatString{parseResult["format"].as<std::string>()};
            if(formatString == "json"){
                config.outputFormat = OutputFormat::JSON;
                config.silent = true;
            }else if(formatString == "csv"){
                config.outputFormat = OutputFormat::CSV;
                config.silent = true;
            }else if(formatString == "keyvalue"){
                config.outputFormat = OutputFormat::KEYVALUE;
                config.silent = true;
            }else if(formatString != "text"){
                throw std::invalid_argument{"Invalid format. Choose from: text, json, csv, or keyvalue"};
            }
        }
        if(parseResult.count("silent")){
            config.silent = parseResult["silent"].as<bool>();
        }

        NaNalyzer nanalyzer;
        return nanalyzer.run(config);
    }catch(const std::exception &exception){
        fmt::println(stderr, "Error: {}", exception.what());
        return 1;
    }
}