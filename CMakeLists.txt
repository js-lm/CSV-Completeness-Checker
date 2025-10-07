cmake_minimum_required(VERSION 3.12)

project(csv-completeness-checker CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)



FetchContent_Declare(
    magic_enum
    GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
    GIT_TAG        v0.9.7
)
FetchContent_MakeAvailable(magic_enum)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        12.0.0
)
FetchContent_MakeAvailable(fmt)

FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        v3.3.1
)
FetchContent_MakeAvailable(cxxopts)

FetchContent_Declare(
    fast_cpp_csv_parser
    GIT_REPOSITORY https://github.com/ben-strasser/fast-cpp-csv-parser.git
)
FetchContent_MakeAvailable(fast_cpp_csv_parser)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)



file(GLOB_RECURSE PROJECT_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/sources/*.cpp"
)



add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/sources"
    ${fast_cpp_csv_parser_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    fmt::fmt
    magic_enum::magic_enum
    cxxopts::cxxopts
    nlohmann_json::nlohmann_json
)